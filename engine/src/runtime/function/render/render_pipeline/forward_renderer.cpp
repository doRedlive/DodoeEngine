#include "runtime/function/render/render_pipeline/forward_renderer.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_pipeline/render_feature/forward_lit_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/transparent_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/shadow_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/skybox_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/post_process_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/present_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/sprite_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/ui_feature.h"
#include "runtime/function/render/render_pipeline/render_feature/imgui_feature.h"
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

    Bool ForwardRenderer::initialize(const RendererCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("ForwardRenderer::initialize", "startup");
        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_shared_render_service = info.shared_render_service;
        DO_ASSERT(m_gfx_context != nullptr, "ForwardRenderer requires valid gfx_context");
        DO_ASSERT(m_shared_render_service != nullptr, "ForwardRenderer requires shared render service");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "ForwardRenderer requires shader library");
        DO_ASSERT(m_shared_render_service->getTextureManager() != nullptr, "ForwardRenderer requires texture manager");

        addFeature<ForwardLitSceneFeature>();
        addFeature<TransparentSceneFeature>();
        addFeature<ShadowSceneFeature>();
        addFeature<SkyboxFeature>();

        addFeature<PostProcessFeature>();
        addFeature<SpriteFeature>();
        addFeature<UIFeature>();
#ifdef DODOE_EDITOR_ENABLED
        addFeature<GizmoFeature>();
#endif//DODOE_EDITOR_ENABLED
        addFeature<ImGuiFeature>();
        addFeature<PresentFeature>();

        bakePasses();

        return true;
    }

    void ForwardRenderer::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("ForwardRenderer::shutdown", "shutdown");
        clearFeatures();
        m_shared_render_service = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    void ForwardRenderer::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        clearViewExtensions(view_family);
        view_family.buildVisiblePrimitives(scene);
    }

    void ForwardRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
                                 UInt32 swapchain_image_index, DrawCommandList& out_commands,
                                 FrameStagingAllocator* frame_staging_allocator,
                                 RenderGraphTransientPool* transient_resource_pool) {
        DO_PROFILE_SCOPE_CATEGORY("ForwardRenderer::render", "frame");
        initViews(scene, view_family);

        DO_PROFILE_MARK("ForwardRenderer::render.setupMeshPassContexts", "frame");
        auto* opaque_feature = getFeature<ForwardLitSceneFeature>();
        DO_ASSERT(opaque_feature != nullptr, "ForwardRenderer ForwardLitSceneFeature is null");
        opaque_feature->setupMeshPassContexts(scene, view_family);

        DO_PROFILE_MARK("ForwardRenderer::render.buildMeshDrawCommands", "frame");
        opaque_feature->buildMeshDrawCommands(view_family, out_commands);

        DO_PROFILE_MARK("ForwardRenderer::render.buildTransparentMeshDrawCommands", "frame");
        auto* transparent_feature = getFeature<TransparentSceneFeature>();
        DO_ASSERT(transparent_feature != nullptr, "ForwardRenderer TransparentSceneFeature is null");
        transparent_feature->buildMeshDrawCommands(view_family, out_commands);

        DO_PROFILE_MARK("ForwardRenderer::render.buildShadowDrawCommands", "frame");
        auto* shadow_feature = getFeature<ShadowSceneFeature>();
        DO_ASSERT(shadow_feature != nullptr, "ForwardRenderer ShadowSceneFeature is null");
        shadow_feature->buildShadowDrawCommands(view_family, out_commands);

        DO_PROFILE_MARK("ForwardRenderer::render.buildOrderedPasses", "frame");
        buildOrderedPasses(view_family, scene, swapchain_image_index, out_commands,
            frame_staging_allocator, transient_resource_pool);
    }

} // namespace dodoe
