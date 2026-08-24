// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/input/backend/input_backend.h"

namespace dodoe {

    class InputBackendGlfw final : public InputBackend {
    public:
        Bool initialize(InputRawState& raw_state, void* native_window) override;
        void shutdown() override;
        void poll(InputRawState& raw_state) override;

    private:
        void onJoystickDisconnected(Int joystick_id);

        static InputBackendGlfw* s_active;
        void* m_window{nullptr};
        InputRawState* m_raw_state{nullptr};
    };

} // namespace dodoe
