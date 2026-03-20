//
// Created by GreenMuffin on 2025/11/08.
//

#include "input_manager.h"

#include "runtime/function/input/input.h"
#include "runtime/function/context.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

namespace dodoe {

    void InputManager::initialize() {
        g_context.event_system->subscribe_event<KeyPressedEvent, &InputManager::on_key_pressed_>(this);
        g_context.event_system->subscribe_event<KeyReleasedEvent, &InputManager::on_key_released_>(this);
        g_context.event_system->subscribe_event<MouseButtonPressedEvent, &InputManager::on_mouse_button_pressed_>(this);
        g_context.event_system->subscribe_event<MouseButtonReleasedEvent, &InputManager::on_mouse_button_released_>(this);
        g_context.event_system->subscribe_event<MouseMovedEvent, &InputManager::on_mouse_moved_>(this);

        Input::initialize(this);
    }

    void InputManager::update() {
        for (auto it = key_map_.begin(); it != key_map_.end(); ) {
            if (auto&[key_code, action_state] = it->second; action_state == ActionState::Pressed) {
                action_state = ActionState::Held;
                ++it;
            } else if (action_state == ActionState::Released) {
                action_state = ActionState::Invalid;
                ++it;
            } else if (action_state == ActionState::Invalid) {
                it = key_map_.erase(it);
            } else {
                ++it;
            }
        }

        if (mouse_info_.action_state == ActionState::Pressed) {
            mouse_info_.action_state = ActionState::Held;
        } else if (mouse_info_.action_state == ActionState::Released) {
            mouse_info_.action_state = ActionState::Invalid;
        } else if (mouse_info_.action_state == ActionState::Invalid) {
            mouse_info_.button = MouseCode::None;
        }
    }

    void InputManager::shutdown() {
        key_map_.clear();

        g_context.event_system->unsubscribe_event<KeyPressedEvent, &InputManager::on_key_pressed_>(this);
        g_context.event_system->unsubscribe_event<KeyReleasedEvent, &InputManager::on_key_released_>(this);
        g_context.event_system->unsubscribe_event<MouseButtonPressedEvent, &InputManager::on_mouse_button_pressed_>(this);
        g_context.event_system->unsubscribe_event<MouseButtonReleasedEvent, &InputManager::on_mouse_button_released_>(this);
        g_context.event_system->unsubscribe_event<MouseMovedEvent, &InputManager::on_mouse_moved_>(this);
    }

    void InputManager::on_key_pressed_(KeyPressedEvent& e) {
        const auto key_code = e.scancode;
        if (key_map_.contains(key_code)) {
            key_map_[key_code].action_state = ActionState::Pressed;
            return;
        }
        const KeyInfo key_info {key_code, ActionState::Pressed};
        key_map_[key_code] = key_info;
    }
    
    void InputManager::on_key_released_(KeyReleasedEvent& e) {
        const auto key_code = static_cast<KeyCode>(e.scancode);
        if (key_map_.contains(key_code)) {
            key_map_[key_code].action_state = ActionState::Released;
            return;
        }
        const KeyInfo key_info {key_code, ActionState::Released};
        key_map_[key_code] = key_info;
    }
    
    void InputManager::on_mouse_button_pressed_(const MouseButtonPressedEvent& e) {
        mouse_info_.action_state = ActionState::Pressed;
        mouse_info_.button = e.button;
    }
    
    void InputManager::on_mouse_button_released_(const MouseButtonReleasedEvent& e) {
        mouse_info_.action_state = ActionState::Released;
        mouse_info_.button = e.button;
    }
    
    void InputManager::on_mouse_moved_(const MouseMovedEvent& e) {
        mouse_info_.position = e.position;
    }

}
