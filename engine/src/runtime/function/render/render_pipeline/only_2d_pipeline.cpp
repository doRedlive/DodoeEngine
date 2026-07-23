// do@Redlive

#include "only_2d_pipeline.h"

namespace dodoe {

    Bool Only2DPipeline::initialize(const RenderPipelineDefinition& definition,
                                    const RendererCreateInfo& info)
    {
        (void)definition;

        auto renderer = create_scope<Only2DRenderer>();
        if (!renderer->initialize(info)) {
            return false;
        }

        m_renderer = std::move(renderer);
        return true;
    }

    void Only2DPipeline::onResize(UInt32 width, UInt32 height) {
        if (m_renderer) {
            m_renderer->onResize(width, height);
        }
    }

    void Only2DPipeline::render(RenderViewFamily& view_family,
                                RenderScene& scene,
                                UInt32 swapchain_image_index,
                                DrawCommandList& out_commands)
    {
        DO_ASSERT(m_renderer != nullptr, "Only2DPipeline has no renderer");
        m_renderer->render(view_family, scene, swapchain_image_index, out_commands);
    }

    void Only2DPipeline::shutdown() {
        if (m_renderer) {
            m_renderer->shutdown();
            m_renderer.reset();
        }
    }

} // namespace dodoe
