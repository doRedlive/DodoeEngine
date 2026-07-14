// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderPipeline : public Managed<RenderPipeline, RendererCreateInfo> {
        friend class Managed<RenderPipeline, RendererCreateInfo>;

        Scope<IRenderer> m_renderer{nullptr};

    public:
        void render(RenderViewFamily& view_family, RenderScene& scene,
                    const UInt32 swapchain_image_index, DrawCommandList& out_commands);

    private:
        Bool initialize(const RendererCreateInfo& info);
        void shutdown();
    };

} // dodoe
