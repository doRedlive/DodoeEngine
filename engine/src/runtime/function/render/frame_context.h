// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    struct FrameContext {
        DrawCommandList command_list;
        UInt32 swapchain_image_index{0};
    };

} // namespace dodoe
