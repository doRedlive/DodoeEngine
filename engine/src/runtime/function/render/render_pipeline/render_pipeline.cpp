// do@Redlive

#include "render_pipeline.h"

#include "deferred_renderer.h"
#include "only_2d_renderer.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    Bool RenderPipeline::initialize(const RendererCreateInfo& info) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        DO_ASSERT(pipeline_type == RenderingPipelineType::Deferred || pipeline_type == RenderingPipelineType::Only2D,
                  "RenderPipeline currently only supports Deferred and Only2D pipeline types");

        switch (pipeline_type) {
        case RenderingPipelineType::Deferred: {
            auto renderer = create_scope<DeferredRenderer>();
            if (!renderer->initialize(info)) {
                return false;
            }
            m_renderer = std::move(renderer);
            break;
        }
        case RenderingPipelineType::Only2D: {
            auto renderer = create_scope<Only2DRenderer>();
            if (!renderer->initialize(info)) {
                return false;
            }
            m_renderer = std::move(renderer);
            break;
        }
        default:
            return false;
        }

        return true;
    }

    void RenderPipeline::shutdown() {
        if (m_renderer) {
            if (auto* deferred = dynamic_cast<DeferredRenderer*>(m_renderer.get())) {
                deferred->shutdown();
            } else if (auto* only2d = dynamic_cast<Only2DRenderer*>(m_renderer.get())) {
                only2d->shutdown();
            }
            m_renderer.reset();
        }
    }

    void RenderPipeline::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        DO_ASSERT(m_renderer != nullptr, "RenderPipeline has no renderer");
        m_renderer->render(view_family, scene, swapchain_image_index, out_commands);
    }

} // dodoe
