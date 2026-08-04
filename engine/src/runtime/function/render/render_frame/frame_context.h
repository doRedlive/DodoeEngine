// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class FrameStagingAllocator;
    class RenderGraphTransientPool;

    struct FrameContext {
        DrawCommandList* command_list{nullptr};
        UInt32 swapchain_image_index{0};
        UInt64 frame_number{0};
        GfxEventQueryHandle completion_query{};
        FrameStagingAllocator* staging{nullptr};
        RenderGraphTransientPool* transient_resource_pool{nullptr};
        Bool valid{false};
    };

} // namespace dodoe
