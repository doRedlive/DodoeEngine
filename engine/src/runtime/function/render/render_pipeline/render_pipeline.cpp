// do@Redlive

#include "render_pipeline.h"

#include "deferred_renderer.h"
#include "only_2d_renderer.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    Bool RenderPipeline::initialize(const RendererCreateInfo& info) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();

        switch (pipeline_type) {
        case RenderingPipelineType::Deferred: {
            auto renderer = create_scope<DeferredRenderer>();
            if (!renderer->initialize(info)) {
                return false;
            }
            m_active_renderer = std::move(renderer);
            return true;
        }
        case RenderingPipelineType::Only2D: {
            auto renderer = create_scope<Only2DRenderer>();
            if (!renderer->initialize(info)) {
                return false;
            }
            m_active_renderer = std::move(renderer);
            return true;
        }
        default:
            DO_ASSERT(false, "RenderPipeline unsupported pipeline type");
            return false;
        }
    }

    void RenderPipeline::shutdown() {
        if (m_active_renderer) {
            m_active_renderer->shutdown();
            m_active_renderer.reset();
        }
    }

    void RenderPipeline::onResize(UInt32 width, UInt32 height) {
        if (m_active_renderer) {
            m_active_renderer->onResize(width, height);
        }
    }

    void RenderPipeline::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        DO_ASSERT(m_active_renderer != nullptr, "RenderPipeline has no active renderer");
        m_active_renderer->render(view_family, scene, swapchain_image_index, out_commands);
    }

} // dodoe
