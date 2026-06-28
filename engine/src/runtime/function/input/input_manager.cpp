//
// Created by GreenMuffin on 2025/11/08.
//

#include "input_manager.h"

#include "runtime/function/input/input.h"
#include "runtime/core/application.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer.h"

namespace dodoe {

    bool InputManager::initialize(const InputManagerInitInfo& init_info) {
        viewport_manager_ = init_info.viewport_manager;

        EventSystem::Subscribe<KeyPressedEvent, &InputManager::on_key_pressed_>(this);
        EventSystem::Subscribe<KeyReleasedEvent, &InputManager::on_key_released_>(this);
        EventSystem::Subscribe<MouseButtonPressedEvent, &InputManager::on_mouse_button_pressed_>(this);
        EventSystem::Subscribe<MouseButtonReleasedEvent, &InputManager::on_mouse_button_released_>(this);
        EventSystem::Subscribe<MouseMovedEvent, &InputManager::on_mouse_moved_>(this);

        Input::initialize(this);
        return true;
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

        EventSystem::Unsubscribe<KeyPressedEvent, &InputManager::on_key_pressed_>(this);
        EventSystem::Unsubscribe<KeyReleasedEvent, &InputManager::on_key_released_>(this);
        EventSystem::Unsubscribe<MouseButtonPressedEvent, &InputManager::on_mouse_button_pressed_>(this);
        EventSystem::Unsubscribe<MouseButtonReleasedEvent, &InputManager::on_mouse_button_released_>(this);
        EventSystem::Unsubscribe<MouseMovedEvent, &InputManager::on_mouse_moved_>(this);

        Input::shutdown();
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

    Vector2f InputManager::window2world(const Vector2f& window_pos) const {
        if (!viewport_manager_) {
            return window_pos;
        }

        const auto& logical_size = viewport_manager_->getLogicalSize();
        const auto& window_size = viewport_manager_->getWindowSize();
        const auto& pixel_size = viewport_manager_->getPixelSize();
        const auto& viewport = viewport_manager_->viewport();

        if (window_size.x <= 0.0f || window_size.y <= 0.0f
            || pixel_size.x <= 0.0f || pixel_size.y <= 0.0f
            || viewport.size.x <= 0.0f || viewport.size.y <= 0.0f) {
            return window_pos;
        }

        const Vector2f pixel_pos{
            window_pos.x * (static_cast<float>(pixel_size.x) / static_cast<float>(window_size.x)),
            window_pos.y * (static_cast<float>(pixel_size.y) / static_cast<float>(window_size.y))
        };

        const float normalized_x = (pixel_pos.x - viewport.pos.x) / viewport.size.x;
        const float normalized_y = (pixel_pos.y - viewport.pos.y) / viewport.size.y;

        const Vector3f logical_pos{
            normalized_x * logical_size.x,
            (1.0f - normalized_y) * logical_size.y,
            0.0f
        };

        const auto* render_system = GetRenderSystem();
        const auto* view_family = render_system ? render_system->getViewFamily() : nullptr;
        if (!view_family || view_family->isEmpty()) {
            return Vector2f(logical_pos.x, logical_pos.y);
        }

        const auto& view = view_family->getView(0);
        const Vector4f clip_pos{
            logical_pos.x / logical_size.x * 2.0f - 1.0f,
            logical_pos.y / logical_size.y * 2.0f - 1.0f,
            0.0f,
            1.0f
        };
        Vector4f world_pos = Math::Inverse(view.getViewProjectionMatrix()) * clip_pos;
        if (std::abs(world_pos.w) > 0.00001f) {
            world_pos /= world_pos.w;
        }
        return Vector2f(world_pos.x, world_pos.y);
    }

}
