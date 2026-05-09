//
// Created by GreenMuffin on 2025/12/10.
//

#include "input.h"

#include "input_manager.h"

namespace dodoe {

    InputManager* Input::input_manager_ = nullptr;

    void Input::initialize(InputManager* input_manager) {
        input_manager_ = input_manager;
    }

    void Input::shutdown() {
        input_manager_ = nullptr;
    }

    bool Input::IsKeyPressed(const KeyCode key_code) {
        if (input_manager_->key_map_.contains(key_code)) {
            if (const auto info = input_manager_->key_map_[key_code]; 
                    info.action_state == ActionState::Pressed || info.action_state == ActionState::Held) {
                return true;
            }
        }
        return false;
    }

    bool Input::IsMouseButtonPressed(const MouseCode mouse_code) {
        if (mouse_code == input_manager_->mouse_info_.button && 
                (input_manager_->mouse_info_.action_state == ActionState::Pressed || 
                input_manager_->mouse_info_.action_state == ActionState::Held)) {
            return true;
        }
        return false;
    }

    Vector2f Input::GetMousePosition() {
        return input_manager_->window2world(input_manager_->mouse_info_.position);
    }

    Vector2f Input::GetMouseWindowPosition() {
        return input_manager_->mouse_info_.position;
    }

    float Input::get_mouse_x() {
        return GetMousePosition().x;
    }

    float Input::get_mouse_y() {
        return GetMousePosition().y;
    }
} // dodoe
