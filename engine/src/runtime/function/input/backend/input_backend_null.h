// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/input/backend/input_backend.h"

namespace dodoe {

    class InputBackendNull final : public InputBackend {
    public:
        Bool initialize(InputRawState&, void*) override { return true; }
        void shutdown() override {}
        void poll(InputRawState&) override {}
    };

} // namespace dodoe
