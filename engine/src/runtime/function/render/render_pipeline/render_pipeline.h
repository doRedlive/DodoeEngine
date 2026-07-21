// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "render_pipeline_base.h"
#include "render_pipeline_registry.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderPipeline : public Managed<RenderPipeline, RendererCreateInfo> {
        friend class Managed<RenderPipeline, RendererCreateInfo>;

        RenderPipelineDefinition m_definition{};
        RenderPipelineRegistry m_registry{};
        RenderPipelineInstance m_active_pipeline{};

    public:
        void onResize(UInt32 width, UInt32 height);
        void render(RenderViewFamily& view_family, RenderScene& scene,
                    const UInt32 swapchain_image_index, DrawCommandList& out_commands);

    private:
        Bool initialize(const RendererCreateInfo& info);
        void shutdown();
    };

} // dodoe
