#pragma once

#include "dopch.h"

#include "runtime/function/render/frame_context.h"

namespace dodoe {

    class GfxContext;

    class DrawExecutor {
    public:
        DrawExecutor() = default;

        void execute(GfxDeviceHandle device, GfxContext* gfx, FrameContext& frame_ctx);
    };

} // dodoe
