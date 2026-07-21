// do@Redlive

#pragma once

#include "dopch.h"

#include "only_2d_renderer.h"
#include "render_pipeline_base.h"

namespace dodoe {

    class Only2DPipeline final : public RenderPipelineBase {
        Scope<Only2DRenderer> m_renderer{nullptr};

    public:
        ~Only2DPipeline() override = default;

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
