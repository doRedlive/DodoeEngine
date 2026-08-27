// do@Redlive

#include "only_gui_renderer.h"

#include "render_feature/present_feature.h"
#include "render_feature/ui_feature.h"
#include "render_feature/imgui_feature.h"

namespace dodoe {

    Bool OnlyGUIRenderer::initialize(const RendererCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("OnlyGUIRenderer::initialize", "startup");
        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_shared_render_service = info.shared_render_service;

        addFeature<UIFeature>();
        addFeature<ImGuiFeature>();
        addFeature<PresentFeature>();

        bakePasses();

        return true;
    }

    void OnlyGUIRenderer::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("OnlyGUIRenderer::shutdown", "shutdown");
        clearFeatures();
        m_shared_render_service = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    void OnlyGUIRenderer::initViews(RenderViewFamily& view_family) const {
        clearViewExtensions(view_family);
    }

    void OnlyGUIRenderer::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands,
                                 FrameStagingAllocator* frame_staging_allocator,
                                 RenderGraphTransientPool* transient_resource_pool) {
        DO_PROFILE_SCOPE_CATEGORY("OnlyGUIRenderer::render", "frame");
        initViews(view_family);
        DO_PROFILE_MARK("OnlyGUIRenderer::render.buildOrderedPasses", "frame");
        buildOrderedPasses(view_family, scene, swapchain_image_index, out_commands,
            frame_staging_allocator, transient_resource_pool);
    }

} // dodoe
