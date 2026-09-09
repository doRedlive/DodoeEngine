// do@Redlive

#include "deferred_renderer.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_pipeline/render_feature/gbuffer_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/shadow_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/skybox_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/lighting_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/post_process_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/present_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/sprite_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/ui_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/imgui_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/test_feature.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#ifdef DODOE_EDITOR_ENABLED
#include "runtime/function/render/render_pipeline/render_feature/gizmo_feature.h"
#endif//DODOE_EDITOR_ENABLED
#include "runtime/function/render/render_scene/render_object.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/utils/common.h"

#include <chrono>
#include <cstdlib>

namespace dodoe {

	Bool DeferredRenderer::initialize(const RendererCreateInfo& info) {
	    DO_PROFILE_SCOPE_CATEGORY("DeferredRenderer::initialize", "startup");
	    Size_t worker_count = info.worker_count;
	    if (worker_count == 0) {
	        worker_count = std::thread::hardware_concurrency();
	    }

	    m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
	    m_gfx_context = info.gfx_context;
	    m_shared_render_service = info.shared_render_service;
	    DO_ASSERT(m_gfx_context != nullptr, "DeferredRenderer requires valid gfx_context");
	    DO_ASSERT(m_shared_render_service != nullptr, "DeferredRenderer requires shared render service");
	    DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "DeferredRenderer requires shader library");
	    DO_ASSERT(m_shared_render_service->getTextureManager() != nullptr, "DeferredRenderer requires texture manager");

	    const auto* shader_library = m_shared_render_service->getShaderLibrary();

	    m_gpu_culling = GpuCulling::Create({m_gfx_context, const_cast<ShaderLibrary*>(shader_library)});

	    // addFeature<TestFeature>();

	    LogEnabledRenderFeatures();
	    addFeature<GBufferSceneFeature>();
	    addFeature<ShadowSceneFeature>();
	    addFeature<SkyboxFeature>();
	    getFeature<GBufferSceneFeature>()->setGpuCulling(m_gpu_culling.get());

	    addFeature<LightingFeature>();
	    addFeature<PostProcessFeature>();
	    addFeature<SpriteFeature>();
	    addFeature<UIFeature>();
#ifdef DODOE_EDITOR_ENABLED
	    //addFeature<GizmoFeature>();
#endif//DODOE_EDITOR_ENABLED
    if (IsRuntimeImGuiEnabled())    addFeature<ImGuiFeature>();
    if (IsRenderFeatureEnabled("present"))  addFeature<PresentFeature>();

	    bakePasses();

	    return true;
	}

	void DeferredRenderer::shutdown() {
	    DO_PROFILE_SCOPE_CATEGORY("DeferredRenderer::shutdown", "shutdown");
	    GpuCulling::Destroy(m_gpu_culling);
	    clearFeatures();
	    m_shared_render_service = nullptr;
	    m_gfx_context = nullptr;
	    m_thread_pool.reset();
	}

	void DeferredRenderer::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
	    clearViewExtensions(view_family);
	    view_family.buildVisiblePrimitives(scene);
	}

	void DeferredRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
	                               const UInt32 swapchain_image_index, DrawCommandList& out_commands,
	                               FrameStagingAllocator* frame_staging_allocator,
	                               RenderGraphTransientPool* transient_resource_pool) {
	    DO_PROFILE_SCOPE_CATEGORY("DeferredRenderer::render", "frame");
	    initViews(scene, view_family);
	    DO_PROFILE_MARK("DeferredRenderer::render.setupMeshPassContexts", "frame");

	    auto* opaque_feature = getFeature<GBufferSceneFeature>();
	    DO_ASSERT(opaque_feature != nullptr, "DeferredRenderer GBufferSceneFeature is null");
	    opaque_feature->setupMeshPassContexts(scene, view_family);

	    auto* shadow_feature = getFeature<ShadowSceneFeature>();
	    DO_ASSERT(shadow_feature != nullptr, "DeferredRenderer ShadowSceneFeature is null");

	    const auto culling_path = RenderSettings::GetFeatureSettings().culling_path;
	    DO_PROFILE_MARK("DeferredRenderer::render.buildMeshDrawCommands", "frame");
	    opaque_feature->buildMeshDrawCommands(view_family, out_commands, getThreadPool());

	    DO_PROFILE_MARK("DeferredRenderer::render.buildShadowDrawCommands", "frame");
	    shadow_feature->buildShadowDrawCommands(view_family, out_commands, getThreadPool());

	    if (culling_path == CullingPath::GpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
	       executeGpuCulling(view_family, scene, out_commands);
	       buildGpuDrivenDrawCommands(scene, view_family, out_commands);
	    }

	    DO_PROFILE_MARK("DeferredRenderer::render.buildOrderedPasses", "frame");
	    buildOrderedPasses(view_family, scene, swapchain_image_index, out_commands,
	        frame_staging_allocator, transient_resource_pool);
	}

	void DeferredRenderer::executeGpuCulling(RenderViewFamily& view_family, RenderScene& scene, DrawCommandList& cmd_list) const {
	    DO_PROFILE_SCOPE_CATEGORY("DeferredRenderer::executeGpuCulling", "frame");
	    if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
	        return;
	    }

	    auto* gpu_scene = scene.getGpuScene();
	    if (!gpu_scene) {
	        return;
	    }

	    const auto scene_resources = gpu_scene->getPassResources();
	    const UInt32 object_count = gpu_scene->getObjectCount();
	    auto* opaque_feature = getFeature<GBufferSceneFeature>();
	    static const DynamicArray<MeshDrawGpuBucket> empty_buckets{};

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        const auto& view = view_family.getView(view_index);
	        DynamicArray<GpuBucketTemplateUpload> bucket_uploads;
	        const auto* opaque_buckets = opaque_feature
	            ? &opaque_feature->getGpuBuckets(view_index) : &empty_buckets;
	        bucket_uploads.reserve(opaque_buckets->size());
	        DynamicArray<GpuBucketDrawSnapshot> snapshots;
	        snapshots.reserve(opaque_buckets->size());
	        const auto& lit_sources = opaque_feature->getLitDrawLists()[view_index].sources;
	        for (const auto& bucket : *opaque_buckets) {
	            const auto& key = bucket.key;
	            const auto& args = bucket.command.getDrawArguments();
                GpuBucketHashKey hash_key{};
                hash_key.type = static_cast<UInt32>(GpuObjectType::Primitive);
                hash_key.material_id = static_cast<UInt32>(key.material);
                hash_key.mesh_id = static_cast<UInt32>(key.vertex_buffer);
	            GpuBucketTemplateUpload upload{};
	            upload.data = GpuBucketTemplate{
	                static_cast<UInt64>(key.pipeline), static_cast<UInt64>(key.material),
	                static_cast<UInt64>(key.binding_set), static_cast<UInt64>(key.vertex_buffer),
	                static_cast<UInt64>(key.index_buffer), args.vertexCount,
                args.startIndexLocation, static_cast<Int32>(args.startVertexLocation),
                bucket.source_count, ComputeGpuBucketHashRaw(hash_key), 0u};
	            upload.hash_bucket = ComputeGpuBucketHash(hash_key, GpuCulling::kMaxBuckets);
	            bucket_uploads.push_back(upload);
	            GpuBucketDrawSnapshot snapshot{};
	            const auto& bucket_cmd = bucket.command;
	            snapshot.pipeline = bucket_cmd.getPipeline();
	            for (const auto& binding_set : bucket_cmd.getBindingSets()) {
	                if (binding_set) {
	                    snapshot.binding_sets.push_back(binding_set);
	                }
	            }
	            snapshot.vertex_bindings = bucket_cmd.getVertexBindings();
	            snapshot.index_binding = bucket_cmd.getIndexBinding();
	            if (bucket.first_source < lit_sources.size()) {
	                snapshot.shader_data = lit_sources[bucket.first_source].shader_data;
	            }
	            snapshots.push_back(std::move(snapshot));
	        }
	        if (!bucket_uploads.empty()) {
	            m_gpu_culling->uploadBucketTemplates(
                cmd_list, bucket_uploads.data(), static_cast<UInt32>(bucket_uploads.size()),
                snapshots.data(), static_cast<UInt32>(snapshots.size()));
	        }
	        static auto last_hash_dump = std::chrono::steady_clock::now() - std::chrono::seconds(2);
	        const auto hash_dump_now = std::chrono::steady_clock::now();
	        if (hash_dump_now - last_hash_dump >= std::chrono::seconds(1)) {
	            last_hash_dump = hash_dump_now;
	            std::string object_dump;
	            DynamicArray<GpuPrimitiveDebugInfo> prim_infos;
	            gpu_scene->getPrimitiveDebugInfos(prim_infos);
	            UInt32 dumped_objects = 0;
	            for (const auto& prim : prim_infos) {
	                if (dumped_objects++ >= 12) break;
	                GpuBucketHashKey key{};
	                key.type = static_cast<UInt32>(GpuObjectType::Primitive);
	                key.material_id = prim.material_id;
	                key.mesh_id = prim.mesh_id;
	                object_dump += " [o" + std::to_string(prim.object_index) +
	                    " f" + std::to_string(prim.flags) +
	                    " mat" + std::to_string(prim.material_id) +
	                    " mesh" + std::to_string(prim.mesh_id) +
	                    " ic" + std::to_string(prim.index_count) +
	                    " si" + std::to_string(prim.start_index) +
	                    " b" + std::to_string(ComputeGpuBucketHash(key, GpuCulling::kMaxBuckets)) + "]";
	            }
	            std::string template_dump;
	            UInt32 dumped_templates = 0;
	            for (const auto& upload : bucket_uploads) {
	                if (dumped_templates++ >= 12) break;
	                template_dump += " [b" + std::to_string(upload.hash_bucket) +
	                    " mat" + std::to_string(static_cast<UInt32>(upload.data.material)) +
	                    " mesh" + std::to_string(static_cast<UInt32>(upload.data.vertex_buffer)) +
	                    " ic" + std::to_string(upload.data.index_count) +
	                    " si" + std::to_string(upload.data.start_index) + "]";
	            }
	            DO_INFO("GpuCulling hash dump objects:{} templates:{}", object_dump, template_dump);
	        }
	        m_gpu_culling->executeCulling(cmd_list, scene_resources,
	                                      view.getViewProjectionMatrix(), object_count);
	        m_gpu_culling->executeBucketBuild(cmd_list, scene_resources, object_count);
	    }
	}

	void DeferredRenderer::buildGpuDrivenDrawCommands(const RenderScene& scene, RenderViewFamily& view_family, DrawCommandList& cmd_list) const {
	    DO_PROFILE_SCOPE_CATEGORY("DeferredRenderer::buildGpuDrivenDrawCommands", "frame");
	    if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
	        return;
	    }

	    auto* gpu_scene = scene.getGpuScene();
	    if (!gpu_scene) {
	        return;
	    }
    (void)view_family;
    (void)cmd_list;
}
} // dodoe
