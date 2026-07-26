// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class FrameStagingAllocator;
    class RenderGraphTransientPool;

    class RenderPipeline : public Managed<RenderPipeline, RendererCreateInfo> {
        friend class Managed<RenderPipeline, RendererCreateInfo>;

        Scope<BaseRenderer> m_active_renderer;

    public:
        void onResize(UInt32 width, UInt32 height);
        void render(RenderViewFamily& view_family, RenderScene& scene,
                    const UInt32 swapchain_image_index, DrawCommandList& out_commands,
                    FrameStagingAllocator* frame_staging_allocator,
                    RenderGraphTransientPool* transient_resource_pool);

    private:
        Bool initialize(const RendererCreateInfo& info);
        void shutdown();
    };

} // dodoe
