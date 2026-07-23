// do@Redlive

#include "renderer.h"

#include "render_graph/render_graph_builder.h"
#include "runtime/function/render/render_view/render_view.h"

namespace dodoe {

    Bool RendererBase::initializeBase(const RendererCreateInfo& info) {
        Size_t worker_count = info.worker_count;
        if (worker_count == 0) {
            worker_count = std::thread::hardware_concurrency();
        }

        m_thread_pool = create_scope<ThreadPool>(std::max(Size_t{1}, worker_count));
        m_gfx_context = info.gfx_context;
        m_shared_render_service = info.shared_render_service;
        DO_ASSERT(m_gfx_context != nullptr, "RendererBase requires valid gfx_context");
        DO_ASSERT(m_shared_render_service != nullptr, "RendererBase requires shared render service");
        DO_ASSERT(m_shared_render_service->getShaderLibrary() != nullptr, "RendererBase requires shader library");
        DO_ASSERT(m_shared_render_service->getDescriptorTable() != nullptr, "RendererBase requires descriptor table");
        DO_ASSERT(m_shared_render_service->getTextureManager() != nullptr, "RendererBase requires texture manager");
        return true;
    }

    void RendererBase::shutdownBase() {
        m_features.clear();
        m_shared_render_service = nullptr;
        m_gfx_context = nullptr;
        m_thread_pool.reset();
    }

    void RendererBase::onResize(const UInt32 width, const UInt32 height) {
        for (const auto& feature : m_features) {
            feature->onResize(width, height);
        }
    }

    RenderPassContext RendererBase::buildPassContext(const RenderScene& scene) const {
        RenderPassContext context{};
        context.gfx_context = m_gfx_context;
        context.shared_render_service = m_shared_render_service;
        context.scene = &scene;
        return context;
    }

    void RendererBase::initViews(const RenderScene& scene, RenderViewFamily& view_family) const {
        for (auto& view : view_family.getViews()) {
            view.resetExtensions();
        }
    }

    void RendererBase::buildFrameDrawCommandList(
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const UInt32 swapchain_image_index,
        DrawCommandList& out_commands) const
    {
        const auto pass_context = buildPassContext(scene);

        DynamicArray<RenderGraphBuilder> graphs;
        graphs.reserve(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            RenderGraphBuilder graph{};
            const auto& view = view_family.getView(view_index);
            const RenderPassBuildContext build_ctx{pass_context, view};
            for (const auto& feature : m_features) {
                feature->registerPass(graph, build_ctx);
            }
            graph.compile();
            graphs.push_back(std::move(graph));
        }

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            executeFrameGraph(graphs[view_index], view_family, scene,
                            view_family.getView(view_index), view_index, swapchain_image_index,
                            out_commands);
        }
    }

    void RendererBase::executeFrameGraph(
        RenderGraphBuilder& graph,
        const RenderViewFamily& view_family,
        RenderScene& scene,
        const RenderView& view,
        const Size_t view_index,
        const UInt32 swapchain_image_index,
        DrawCommandList& out_commands) const
    {
        RenderGraphExecuteContext context{};
        context.view_family = &view_family;
        context.scene = &scene;
        context.view = &view;
        context.view_index = view_index;
        context.gfx_context = m_gfx_context;
        context.shared_render_service = m_shared_render_service;
        context.swapchain_image_index = swapchain_image_index;
        graph.execute(*m_thread_pool, context, out_commands);
    }

} // dodoe
