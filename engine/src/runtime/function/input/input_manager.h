// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/event/event.h"
#include "runtime/core/memory/managed.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"


namespace dodoe {

    enum class ActionState {
        Invalid,
        Pressed,
        Released,
        Held,
    };

    struct KeyInfo {
        KeyCode key_code;
        ActionState action_state;
    };

    struct MouseInfo {
        MouseCode button;
        Vector2f position;
        ActionState action_state;
    };

    struct InputManagerInitInfo {};

    class InputManager : public Managed<InputManager, InputManagerInitInfo> {
        friend class Input;
        friend class Managed<InputManager, InputManagerInitInfo>;

    public:
        void update();

    private:
        [[nodiscard]] bool initialize(const InputManagerInitInfo& init_info);
        void shutdown();

        std::unordered_map<KeyCode, KeyInfo> key_map_{};
        MouseInfo mouse_info_{};

        void on_key_pressed_(KeyPressedEvent& e);
        void on_key_released_(KeyReleasedEvent& e);
        void on_mouse_button_pressed_(const MouseButtonPressedEvent& e);
        void on_mouse_button_released_(const MouseButtonReleasedEvent& e);
        void on_mouse_moved_(const MouseMovedEvent& e);
    };

} // dodoe