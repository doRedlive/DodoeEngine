// do@Redlive

#include "renderer.h"

#include "render_graph/render_graph_builder.h"
#include "runtime/function/render/render_view/render_view.h"

namespace dodoe {

    RenderPassContext BaseRenderer::makePassContext(const RenderScene& scene) const {
        RenderPassContext context{};
        context.gfx_context           = m_gfx_context;
        context.shared_render_service = m_shared_render_service;
        context.scene                 = &scene;
        return context;
    }

    void BaseRenderer::clearViewExtensions(RenderViewFamily& view_family) const {
        for (auto& view : view_family.getViews()) {
            view.resetExtensions();
        }
    }

    void BaseRenderer::setupFramePasses(RenderViewFamily& view_family,
                                        RenderScene& scene,
                                        const UInt32 swapchain_image_index,
                                        DrawCommandList& out_commands) const {
        const auto pass_context = makePassContext(scene);

        DynamicArray<RenderGraphBuilder> graphs;
        graphs.reserve(view_family.getSize());

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            RenderGraphBuilder graph{};
            const auto& view = view_family.getView(view_index);
            const RenderPassBuildContext build_ctx{pass_context, view};
            for (const auto& feature : m_features) {
                feature->setupPasses(graph, build_ctx);
            }
            graph.compile();
            graphs.push_back(std::move(graph));
        }

        for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
            RenderGraphExecuteContext context{};
            context.view_family           = &view_family;
            context.scene                 = &scene;
            context.view                  = &view_family.getView(view_index);
            context.view_index            = view_index;
            context.gfx_context           = m_gfx_context;
            context.shared_render_service = m_shared_render_service;
            context.swapchain_image_index  = swapchain_image_index;
            graphs[view_index].execute(*m_thread_pool, context, out_commands);
        }
    }

} // dodoe
