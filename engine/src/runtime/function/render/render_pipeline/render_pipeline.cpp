// do@Redlive

#include "render_pipeline.h"

#include "deferred_pipeline.h"
#include "only_2d_pipeline.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    namespace {
        template <typename TPipeline>
        RenderPipelineInstance CreatePipelineInstance() {
            auto pipeline = create_scope<TPipeline>();
            auto* raw_pipeline = pipeline.release();
            return {
                raw_pipeline,
                [](RenderPipelineBase* instance) {
                    Scope<TPipeline> owned(static_cast<TPipeline*>(instance));
                    owned.reset();
                }
            };
        }

        String ResolvePipelineTypeName(const RenderingPipelineType pipeline_type) {
            switch (pipeline_type) {
            case RenderingPipelineType::Deferred:
                return "Deferred";
            case RenderingPipelineType::Only2D:
                return "Only2D";
            case RenderingPipelineType::Forward:
                return "Forward";
            case RenderingPipelineType::ForwardPlus:
                return "ForwardPlus";
            case RenderingPipelineType::DeferredPlus:
                return "DeferredPlus";
            default:
                return {};
            }
        }
    } // namespace

    Bool RenderPipeline::initialize(const RendererCreateInfo& info) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        DO_ASSERT(pipeline_type == RenderingPipelineType::Deferred || pipeline_type == RenderingPipelineType::Only2D,
                  "RenderPipeline currently only supports Deferred and Only2D pipeline types");

        m_definition.pipeline_type = ResolvePipelineTypeName(pipeline_type);
        m_definition.culling_path = RenderSettings::GetFeatureSettings().culling_path;

        m_registry.registerFactory("Deferred", []() {
            return CreatePipelineInstance<DeferredPipeline>();
        });
        m_registry.registerFactory("Only2D", []() {
            return CreatePipelineInstance<Only2DPipeline>();
        });

        m_active_pipeline = m_registry.resolve(m_definition);
        if (!m_active_pipeline) {
            return false;
        }

        if (!m_active_pipeline.pipeline->initialize(m_definition, info)) {
            m_active_pipeline.reset();
            return false;
        }

        return m_active_pipeline.isValid();
    }

    void RenderPipeline::shutdown() {
        if (m_active_pipeline) {
            m_active_pipeline.pipeline->shutdown();
            m_active_pipeline.reset();
        }
    }

    void RenderPipeline::onResize(UInt32 width, UInt32 height) {
        if (m_active_pipeline) {
            m_active_pipeline.pipeline->onResize(width, height);
        }
    }

    void RenderPipeline::render(RenderViewFamily& view_family, RenderScene& scene,
                                 const UInt32 swapchain_image_index, DrawCommandList& out_commands) {
        DO_ASSERT(m_active_pipeline.pipeline != nullptr, "RenderPipeline has no active pipeline");
        m_active_pipeline.pipeline->render(view_family, scene, swapchain_image_index, out_commands);
    }

} // dodoe
