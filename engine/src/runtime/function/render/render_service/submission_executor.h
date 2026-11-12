// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class SubmissionExecutor {
    public:
        static void prepareResources(DrawCommandList& commands, GfxDeviceHandle device);
        static void replay(DrawCommandList& commands, GfxCommandList& target);
        static void prepareAndReplay(DrawCommandList& commands, GfxDeviceHandle device, GfxCommandList& target);
    };

} // namespace dodoe
