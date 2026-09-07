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

	    addFeature<GBufferSceneFeature>();
	    addFeature<ShadowSceneFeature>();
	    addFeature<SkyboxFeature>();

	    addFeature<LightingFeature>();
	    addFeature<PostProcessFeature>();
	    addFeature<SpriteFeature>();
	    addFeature<UIFeature>();
#ifdef DODOE_EDITOR_ENABLED
	    //addFeature<GizmoFeature>();
#endif//DODOE_EDITOR_ENABLED
	    addFeature<ImGuiFeature>();
	    addFeature<PresentFeature>();

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

	    const auto culling_path = RenderSettings::GetFeatureSettings().culling_path;
	    if (culling_path == CullingPath::GpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
	       executeGpuCulling(view_family, scene, out_commands);
	       buildGpuDrivenDrawCommands(scene, view_family, out_commands);
	    }

	    if (culling_path == CullingPath::CpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
	       DO_PROFILE_MARK("DeferredRenderer::render.buildMeshDrawCommands", "frame");
	       opaque_feature->buildMeshDrawCommands(view_family, out_commands, getThreadPool());
	    }

	    DO_PROFILE_MARK("DeferredRenderer::render.buildShadowDrawCommands", "frame");
	    auto* shadow_feature = getFeature<ShadowSceneFeature>();
	    DO_ASSERT(shadow_feature != nullptr, "DeferredRenderer ShadowSceneFeature is null");
	    shadow_feature->buildShadowDrawCommands(view_family, out_commands, getThreadPool());

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
	        for (const auto& bucket : *opaque_buckets) {
	            const auto& key = bucket.key;
	            const auto& args = bucket.command.getDrawArguments();
	            GpuBucketHashKey hash_key{};
	            hash_key.type = static_cast<UInt32>(GpuObjectType::Primitive);
	            hash_key.index_count = args.vertexCount;
	            hash_key.start_index = args.startIndexLocation;
	            GpuBucketTemplateUpload upload{};
	            upload.data = GpuBucketTemplate{
	                static_cast<UInt64>(key.pipeline), static_cast<UInt64>(key.material),
	                static_cast<UInt64>(key.binding_set), static_cast<UInt64>(key.vertex_buffer),
	                static_cast<UInt64>(key.index_buffer), args.vertexCount,
                args.startIndexLocation, static_cast<Int32>(args.startVertexLocation),
                bucket.source_count};
	            upload.hash_bucket = ComputeGpuBucketHash(hash_key, GpuCulling::kMaxBuckets);
	            bucket_uploads.push_back(upload);
	        }
	        if (!bucket_uploads.empty()) {
	            m_gpu_culling->uploadBucketTemplates(
                cmd_list, bucket_uploads.data(), static_cast<UInt32>(bucket_uploads.size()));
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

	    const auto indirect_args = m_gpu_culling->getIndirectArgsBuffer(1);
	    if (!indirect_args) {
	        return;
	    }

	    auto* opaque_feature = getFeature<GBufferSceneFeature>();
	    DO_ASSERT(opaque_feature != nullptr, "DeferredRenderer GBufferSceneFeature is null");
	    auto* shadow_feature = getFeature<ShadowSceneFeature>();
	    DO_ASSERT(shadow_feature != nullptr, "DeferredRenderer ShadowSceneFeature is null");
	    const UInt32 object_count = gpu_scene->getObjectCount();

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        auto& view = view_family.getView(view_index);
	        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

	        const auto viewport = GfxViewportState()
	            .addViewportAndScissorRect(GfxViewport(
                static_cast<Float>(view.getViewportRect().z),
                static_cast<Float>(view.getViewportRect().w)));

	        const auto& gpu_buckets = opaque_feature->getGpuBuckets(view_index);
	        DynamicArray<GpuBucketCpuDraw> gpu_draws;
	        const Bool has_bucket_draws = !gpu_buckets.empty() && m_gpu_culling->acquireBucketDraws(gpu_draws);

	        for (Size_t pass_idx = 0; pass_idx < static_cast<Size_t>(MeshPassType::Shadow) + 1; pass_idx++) {
	            if (pass_idx == static_cast<Size_t>(MeshPassType::Opaque)) {
	                if (!has_bucket_draws) {
	                    continue;
	                }
	                for (const auto& draw : gpu_draws) {
	                    if (draw.template_index >= gpu_buckets.size()) {
	                        continue;
	                    }
	                    const auto& bucket_cmd = gpu_buckets[draw.template_index].command;
	                    if (!bucket_cmd.getPipeline()) {
	                        continue;
	                    }

	                    auto graphics_state = GfxGraphicsState()
	                        .setViewport(viewport)
	                        .setPipeline(bucket_cmd.getPipeline()->getRHIHandle());

	                    ShaderParameterBinder binder;
	                    binder.bind(graphics_state, bucket_cmd.getBindingSets());

	                    for (const auto& vertex_binding : bucket_cmd.getVertexBindings()) {
	                        graphics_state.addVertexBuffer(vertex_binding);
	                    }

	                    const auto gpu_scene_resources = gpu_scene->getPassResources();
	                    if (gpu_scene_resources.primitive_instance && gpu_scene_resources.primitive_instance->getRHI()) {
	                        graphics_state.addVertexBuffer(
	                            GfxVertexBufferBinding()
	                                .setBuffer(gpu_scene_resources.primitive_instance->getRHI())
	                                .setSlot(1)
	                                .setOffset(0)
	                        );
	                    }

	                    graphics_state.setIndexBuffer(bucket_cmd.getIndexBinding());
	                    cmd_list.setGraphicsState(graphics_state);

	                    cmd_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
	                    cmd_list.commitBarriers();
	                    cmd_list.drawIndexedIndirect(
	                        draw.first_arg * sizeof(DrawIndexedIndirectArgs), draw.arg_count);
	                }
	                continue;
	            }

	            const auto& draw_lists = shadow_feature->getShadowDrawLists();
	            const auto& mesh_draw_cache = shadow_feature->getMeshDrawCache();
	            const auto& instances = draw_lists[view_index].cached_instances;
	            if (instances.empty()) {
	                continue;
	            }

	            if (instances[0].cmd_index >= mesh_draw_cache.size()) {
	                DO_ERROR("DeferredRenderer: cached mesh draw index out of range");
	                continue;
	            }
	            const auto& cached_cmd = mesh_draw_cache.getCommand(instances[0].cmd_index);
	            if (!cached_cmd.getPipeline()) {
	                DO_ERROR("DeferredRenderer: cached mesh draw has no pipeline");
	                continue;
	            }

	            const auto template_key = cached_cmd.getBucketKey();
	            Bool compatible_templates = true;
	            for (const auto& instance : instances) {
	                if (instance.cmd_index >= mesh_draw_cache.size() ||
	                    !(mesh_draw_cache.getCommand(instance.cmd_index).getBucketKey() == template_key)) {
	                    compatible_templates = false;
	                    break;
	                }
	            }
	            if (!compatible_templates) {
	                DO_WARN("DeferredRenderer: skipping GPU indirect draw with incompatible mesh bucket templates");
	                continue;
	            }

	            auto graphics_state = GfxGraphicsState()
	                .setViewport(viewport)
	                .setPipeline(cached_cmd.getPipeline()->getRHIHandle());

	            ShaderParameterBinder binder;
	            binder.bind(graphics_state, cached_cmd.getBindingSets());

	            for (const auto& vertex_binding : cached_cmd.getVertexBindings()) {
	                graphics_state.addVertexBuffer(vertex_binding);
	            }

	            const auto gpu_scene_resources = gpu_scene->getPassResources();
	            if (gpu_scene_resources.primitive_instance && gpu_scene_resources.primitive_instance->getRHI()) {
	                graphics_state.addVertexBuffer(
	                    GfxVertexBufferBinding()
	                        .setBuffer(gpu_scene_resources.primitive_instance->getRHI())
	                        .setSlot(1)
	                        .setOffset(instances[0].instance_offset)
	                );
	            }

	            graphics_state.setIndexBuffer(cached_cmd.getIndexBinding());
	            cmd_list.setGraphicsState(graphics_state);

	            cmd_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
	            cmd_list.commitBarriers();
	            cmd_list.drawIndexedIndirect(0, object_count);
	        }
	    }
	}

} // dodoe
