// do@Redlive

#include "only_2d_renderer.h"

#include "render_feature/post_process_2d_feature.h"
#include "render_feature/present_feature.h"
#include "render_feature/sprite_feature.h"
#include "render_feature/ui_feature.h"
#include "render_feature/imgui_feature.h"
#ifdef DODOE_EDITOR_ENABLED
#include "render_feature/gizmo_feature.h"
#endif//DODOE_EDITOR_ENABLED

namespace dodoe {

    Bool Only2DRenderer::initialize(const RendererCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("Only2DRenderer::initialize", "startup");
        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_shared_render_service = info.shared_render_service;

        addFeature<SpriteFeature>();
        addFeature<UIFeature>();
        addFeature<PostProcess2DFeature>();
#ifdef DODOE_EDITOR_ENABLED
	    addFeature<GizmoFeature>();
#endif//DODOE_EDITOR_ENABLED
        addFeature<ImGuiFeature>();
        addFeature<PresentFeature>();

        bakePasses();

        return true;
    }

    void Only2DRenderer::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("Only2DRenderer::shutdown", "shutdown");
        clearFeatures();
        m_shared_render_service = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    void Only2DRenderer::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        clearViewExtensions(view_family);
        view_family.buildVisibleSprites(scene);
    }

    void Only2DRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands,
                                 FrameStagingAllocator* frame_staging_allocator,
                                 RenderGraphTransientPool* transient_resource_pool) {
        DO_PROFILE_SCOPE_CATEGORY("Only2DRenderer::render", "frame");
        initViews(scene, view_family);
        DO_PROFILE_MARK("Only2DRenderer::render.buildOrderedPasses", "frame");
        buildOrderedPasses(view_family, scene, swapchain_image_index, out_commands,
            frame_staging_allocator, transient_resource_pool);
    }

} // dodoe
