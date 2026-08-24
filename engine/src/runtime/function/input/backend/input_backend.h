// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/input/input_types.h"

namespace dodoe {

    class InputBackend {
    public:
        virtual ~InputBackend() = default;
        virtual Bool initialize(InputRawState& raw_state, void* native_window) = 0;
        virtual void shutdown() = 0;
        virtual void poll(InputRawState& raw_state) = 0;
    };

} // namespace dodoe
