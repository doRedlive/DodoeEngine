//
// Created by GreenMuffin on 2025/11/08.
//
#ifndef DODOE_INPUT_MANAGER_H
#define DODOE_INPUT_MANAGER_H
#include "dopch.h"

#include "runtime/core/event/event.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include "runtime/function/render/framework/viewport_manager.h"

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

    struct InputManagerInitInfo {
        ViewportManager* viewport_manager;
    };

    class InputManager {
        friend class Input;
    public:
        void initialize(const InputManagerInitInfo& init_info);
        void shutdown();
        void update();

    private:
        std::unordered_map<KeyCode, KeyInfo> key_map_{};
        MouseInfo mouse_info_{};

        ViewportManager* viewport_manager_{nullptr};

        void on_key_pressed_(KeyPressedEvent& e);
        void on_key_released_(KeyReleasedEvent& e);
        void on_mouse_button_pressed_(const MouseButtonPressedEvent& e);
        void on_mouse_button_released_(const MouseButtonReleasedEvent& e);
        void on_mouse_moved_(const MouseMovedEvent& e);

        Vector2f window2world(const Vector2f& window_pos) const;
    };

}
#endif // DODOE_INPUT_MANAGER_H
