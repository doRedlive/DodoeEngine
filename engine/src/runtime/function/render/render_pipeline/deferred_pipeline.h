// do@Redlive

#pragma once

#include "dopch.h"

#include "deferred_renderer.h"
#include "render_pipeline_base.h"

namespace dodoe {

    class DeferredPipeline final : public RenderPipelineBase {
        Scope<DeferredRenderer> m_renderer{nullptr};

    public:
        ~DeferredPipeline() override = default;

        Bool initialize(const RenderPipelineDefinition& definition,
                        const RendererCreateInfo& info) override;

        void onResize(UInt32 width, UInt32 height) override;

        void render(RenderViewFamily& view_family,
                    RenderScene& scene,
                    UInt32 swapchain_image_index,
                    DrawCommandList& out_commands) override;

        void shutdown() override;
    };

} // namespace dodoe
