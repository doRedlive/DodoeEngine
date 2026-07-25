// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class FrameStagingAllocator;

    struct FrameContext {
        DrawCommandList command_list{};
        UInt32 swapchain_image_index{0};
        UInt64 frame_number{0};
        GfxEventQueryHandle completion_query{};
        FrameStagingAllocator* staging{nullptr};
        Bool valid{false};
    };

} // namespace dodoe
