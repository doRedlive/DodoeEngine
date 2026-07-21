// do@Redlive

#include "deferred_pipeline.h"

namespace dodoe {

    Bool DeferredPipeline::initialize(const RenderPipelineDefinition& definition,
                                      const RendererCreateInfo& info)
    {
        (void)definition;

        auto renderer = create_scope<DeferredRenderer>();
        if (!renderer->initialize(info)) {
            return false;
        }

        m_renderer = std::move(renderer);
        return true;
    }

    void DeferredPipeline::onResize(UInt32 width, UInt32 height) {
        (void)width;
        (void)height;
    }

    void DeferredPipeline::render(RenderViewFamily& view_family,
                                  RenderScene& scene,
                                  UInt32 swapchain_image_index,
                                  DrawCommandList& out_commands)
    {
        DO_ASSERT(m_renderer != nullptr, "DeferredPipeline has no renderer");
        m_renderer->render(view_family, scene, swapchain_image_index, out_commands);
    }

    void DeferredPipeline::shutdown() {
        if (m_renderer) {
            m_renderer->shutdown();
            m_renderer.reset();
        }
    }

} // namespace dodoe
