// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"

namespace dodoe {

    class Only2DRenderer final : public BaseRenderer {

        void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;

    public:
        ~Only2DRenderer() override = default;

        Bool initialize(const RendererCreateInfo& info);
        void shutdown();

        void render(RenderViewFamily& view_family, RenderScene& scene,
                    UInt32 swapchain_image_index, DrawCommandList& out_commands,
                    FrameStagingAllocator* frame_staging_allocator) override;
    };

} // dodoe
