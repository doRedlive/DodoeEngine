// do@Redlive

#include "deferred_renderer.h"

#include "render_graph/render_graph_builder.h"
#include "render_pipeline_pass_utils.h"
#include "render_feature/base_scene_feature.h"
#include "render_feature/lighting_feature.h"
#include "render_feature/post_process_feature.h"
#include "render_feature/present_feature.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/imgui_feature.h"
#ifdef DODOE_EDITOR_ENABLED
#include "render_feature/gizmo_feature.h"
#endif
#include "runtime/function/render/render_scene/render_object.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

	Bool DeferredRenderer::initialize(const RendererCreateInfo& info) {
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

	    addFeature<BaseSceneFeature>();

	    addFeature<LightingFeature>();
	    addFeature<PostProcessFeature>();
	    addFeature<SpriteFeature>();
	#ifdef DODOE_EDITOR_ENABLED
	    addFeature<GizmoFeature>();
	#endif
	    addFeature<ImGuiFeature>();
	    addFeature<PresentFeature>();

	    bakePasses();

	    return true;
	}

	void DeferredRenderer::shutdown() {
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
	    initViews(scene, view_family);

	    auto* base_feature = getFeature<BaseSceneFeature>();
	    DO_ASSERT(base_feature != nullptr, "DeferredRenderer BaseSceneFeature is null");
	    base_feature->setupMeshPassContexts(scene, view_family);

	    const auto culling_path = RenderSettings::GetFeatureSettings().culling_path;
	    if (culling_path == CullingPath::GpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
	        executeGpuCulling(view_family, scene, out_commands);
	        buildGpuDrivenDrawCommands(scene, view_family, out_commands);
	    }

	    if (culling_path == CullingPath::CpuOnly || culling_path == CullingPath::CpuThenGpuVerify) {
	        base_feature->buildMeshDrawCommands(view_family, out_commands);
	    }

	    buildOrderedPasses(view_family, scene, swapchain_image_index, out_commands,
	        frame_staging_allocator, transient_resource_pool);
	}

	void DeferredRenderer::executeGpuCulling(RenderViewFamily& view_family, RenderScene& scene, DrawCommandList& cmd_list) const {
	    if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
	        return;
	    }

	    auto* gpu_scene = scene.getGpuScene();
	    if (!gpu_scene) {
	        return;
	    }

	    const auto scene_resources = gpu_scene->getPassResources();
	    const UInt32 object_count = gpu_scene->getObjectCount();

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        const auto& view = view_family.getView(view_index);
	        m_gpu_culling->executeCulling(cmd_list, scene_resources,
	                                      view.getViewProjectionMatrix(), object_count);
	        m_gpu_culling->executeBucketBuild(cmd_list, scene_resources, object_count);
	    }
	}

	void DeferredRenderer::buildGpuDrivenDrawCommands(const RenderScene& scene, RenderViewFamily& view_family, DrawCommandList& cmd_list) const {
	    if (!m_gpu_culling || !m_gpu_culling->isEnabled()) {
	        return;
	    }

	    auto* gpu_scene = scene.getGpuScene();
	    if (!gpu_scene) {
	        return;
	    }

	    const auto indirect_args = m_gpu_culling->getIndirectArgsBuffer();
	    if (!indirect_args) {
	        return;
	    }

	    auto* base_feature = getFeature<BaseSceneFeature>();
	    DO_ASSERT(base_feature != nullptr, "DeferredRenderer BaseSceneFeature is null");
	    const auto& mesh_draw_cache = base_feature->getMeshDrawCache();

	    const UInt32 object_count = gpu_scene->getObjectCount();

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        auto& view = view_family.getView(view_index);
	        auto& mesh_ext = view.getOrCreateExtension<MeshViewExtension>();

	        const auto viewport = GfxViewportState()
	            .setViewport(view.getViewportWidth(), view.getViewportHeight());

	        for (Size_t pass_idx = 0; pass_idx < static_cast<Size_t>(MeshPassType::Count); pass_idx++) {
                const auto& draw_lists = (pass_idx == static_cast<Size_t>(MeshPassType::GBuffer))
                    ? base_feature->getGBufferDrawList() : base_feature->getShadowDrawLists();
	            const auto& instances = draw_lists[view_index].cached_intances;
	            if (instances.empty()) {
	                continue;
	            }

	            const auto& cached_cmd = mesh_draw_cache.getCommand(instances[0].cmd_index);

	            auto graphics_state = GfxGraphicsState()
	                .setViewport(viewport)
	                .setPipeline(cached_cmd.pipeline->getRHIHandle());

	            for (const auto& binding_set : cached_cmd.binding_sets) {
	                if (binding_set && binding_set->isRHIReady()) {
	                    graphics_state.addBindingSet(binding_set->getRHIHandle());
	                }
	            }

	            for (const auto& vertex_binding : cached_cmd.vertex_bindings) {
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

	            graphics_state.setIndexBuffer(cached_cmd.index_binding);
	            cmd_list.setGraphicsState(graphics_state);

	            cmd_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
	            cmd_list.commitBarriers();
	            cmd_list.drawIndexedIndirect(0, object_count);
	        }
	    }
	}

} // dodoe
