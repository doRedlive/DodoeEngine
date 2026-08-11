//
// Created by Redlive on 2026/3/23.
//

#pragma once

#include "dopch.h"

namespace dodoe {

    struct AnimClipEvent {
        Float time{0.0f};
        String function_name{};

        AnimClipEvent() = default;
        AnimClipEvent(const Float in_time, const String& in_function_name)
            : time(in_time), function_name(in_function_name) {}
    };

    class Animation {

    };

} // dodoe
