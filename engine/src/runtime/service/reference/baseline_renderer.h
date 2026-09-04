// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    struct BaselineRendererCreateInfo {
        GfxDeviceHandle device{};
    };

    class BaselineRenderer final : public Managed<BaselineRenderer, BaselineRendererCreateInfo> {
        friend class Managed<BaselineRenderer, BaselineRendererCreateInfo>;
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        UInt64 m_frame_counter{0};

    public:
        void render(GfxContext& gfx, UInt32 swapchain_image_index);

    private:
        Bool initialize(const BaselineRendererCreateInfo& info);
        void shutdown();

    };

} // dodoe