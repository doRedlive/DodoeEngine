// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"

namespace dodoe {

    class OnlyGUIRenderer final : public BaseRenderer {

        void initViews(RenderViewFamily& view_family) const;

    public:
        ~OnlyGUIRenderer() override = default;

        Bool initialize(const RendererCreateInfo& info);
        void shutdown() override;

        void render(RenderViewFamily& view_family, RenderScene& scene,
                    UInt32 swapchain_image_index, DrawCommandList& out_commands,
                    FrameStagingAllocator* frame_staging_allocator,
                    RenderGraphTransientPool* transient_resource_pool) override;
    };

} // dodoe
