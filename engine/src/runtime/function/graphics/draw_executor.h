#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class GfxContext;

    class DrawExecutor {
    public:
        DrawExecutor() = default;

        void execute(GfxDeviceHandle device, GfxContext* gfx, UInt32 swapchain_image_index);
    };

} // dodoe
