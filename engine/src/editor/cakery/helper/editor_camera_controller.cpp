// do@Redlive

#include "editor_camera_controller.h"

#include "runtime/core/event/event_system.h"
#include "runtime/function/input/input.h"
#include "runtime/function/input/mouse_code.h"

namespace cakery {

    dodoe::Scope<EditorCameraController> EditorCameraController::Create(const EditorCameraControllerCreateInfo &info) {
        if (auto controller = dodoe::create_scope<EditorCameraController>(); controller->initialize(info))
            return controller;
        return nullptr;
    }

    void EditorCameraController::Destroy(dodoe::Scope<EditorCameraController> &controller) {
        if (!controller) return;
        controller->shutdown();
        controller.reset();
    }

    void EditorCameraController::onUpdate(const float dt) {
        if (!m_camera) return;

        if (m_camera->getCameraType() == dodoe::CameraType::Orthographic) {
            update2D(dt);
        } else {
            update3D(dt);
        }
    }

    void EditorCameraController::update2D(const float dt) {
        (void)dt;

        const dodoe::Vector2f mouse_position = dodoe::Input::GetMouseWindowPosition();
        const bool middle_pressed = dodoe::Input::IsMouseButtonPressed(dodoe::MouseCode::ButtonMiddle);
        const bool right_pressed = dodoe::Input::IsMouseButtonPressed(dodoe::MouseCode::ButtonRight);
        const bool dragging = middle_pressed || right_pressed;

        if (dragging) {
            if (m_middle_dragging) {
                const dodoe::Vector2f delta = mouse_position - m_last_mouse_position;
                const float zoom = m_camera->getZoom();
                const auto& logical_size = m_camera->getLogicalSize();
                const auto& window_size = m_camera->getWindowSize();
                const float scale_x = logical_size.x / (window_size.x > 0 ? static_cast<float>(window_size.x) : 1.0f);
                const float scale_y = logical_size.y / (window_size.y > 0 ? static_cast<float>(window_size.y) : 1.0f);
                m_camera->translate(dodoe::Vector3f(-delta.x * scale_x / zoom, delta.y * scale_y / zoom, 0.0f));
            }
            m_middle_dragging = true;
        } else {
            m_middle_dragging = false;
        }

        if (m_scroll_delta_y != 0.0f) {
            const float zoom = m_camera->getZoom();
            const float zoom_factor = 1.0f + m_scroll_delta_y * 0.1f;
            const float new_zoom = std::clamp(zoom * zoom_factor, 0.01f, 100.0f);
            m_camera->setZoom(new_zoom);
            m_scroll_delta_y = 0.0f;
        }

        m_last_mouse_position = mouse_position;
    }

    void EditorCameraController::update3D(const float dt) {
        const auto make_forward = [this]() {
            return dodoe::Math::Normalize(dodoe::Vector3f(
                std::cos(m_pitch) * std::cos(m_yaw),
                std::sin(m_pitch),
                std::cos(m_pitch) * std::sin(m_yaw)
            ));
        };
        const auto make_right = [](const dodoe::Vector3f& forward) {
            return dodoe::Math::Normalize(dodoe::Math::Cross(forward, dodoe::Vector3f(0.0f, 1.0f, 0.0f)));
        };

        float speed_scale = 1.0f;
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::LeftShift) ||
            dodoe::Input::IsKeyPressed(dodoe::KeyCode::RightShift)) {
            speed_scale *= m_fast_speed_multiplier;
        }
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::LeftControl) ||
            dodoe::Input::IsKeyPressed(dodoe::KeyCode::RightControl)) {
            speed_scale *= m_slow_speed_multiplier;
        }

        dodoe::Vector2f move_axis(0.0f, 0.0f);
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::W) || dodoe::Input::IsKeyPressed(dodoe::KeyCode::Up)) {
            move_axis.y += 1.0f;
        }
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::S) || dodoe::Input::IsKeyPressed(dodoe::KeyCode::Down)) {
            move_axis.y -= 1.0f;
        }
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::A) || dodoe::Input::IsKeyPressed(dodoe::KeyCode::Left)) {
            move_axis.x -= 1.0f;
        }
        if (dodoe::Input::IsKeyPressed(dodoe::KeyCode::D) || dodoe::Input::IsKeyPressed(dodoe::KeyCode::Right)) {
            move_axis.x += 1.0f;
        }

        if (move_axis.x != 0.0f || move_axis.y != 0.0f) {
            move_axis = dodoe::Math::Normalize(move_axis);
            const dodoe::Vector3f forward = make_forward();
            const dodoe::Vector3f right = make_right(forward);

            const dodoe::Vector3f move_delta =
                (right * move_axis.x + forward * move_axis.y) *
                (m_move_speed * speed_scale * dt);
            m_camera->translate(move_delta);
        }

        if (m_scroll_delta_y != 0.0f) {
            m_camera->translate(m_camera->getViewDirection() * (m_scroll_delta_y * m_zoom_speed));
            m_scroll_delta_y = 0.0f;
        }

        const dodoe::Vector2f mouse_position = dodoe::Input::GetMouseWindowPosition();
        const bool right_pressed = dodoe::Input::IsMouseButtonPressed(dodoe::MouseCode::ButtonRight);

        if (right_pressed) {
            if (m_right_dragging) {
                const dodoe::Vector2f delta = mouse_position - m_last_mouse_position;
                m_yaw += delta.x * m_rotate_speed;
                m_pitch -= delta.y * m_rotate_speed;
                const float pitch_limit = dodoe::Math::Radians(89.0f);
                m_pitch = dodoe::Math::Clamp(m_pitch, -pitch_limit, pitch_limit);
            }
            m_right_dragging = true;
        }
        else {
            m_right_dragging = false;
        }

        m_camera->setViewDirection(make_forward());
        m_last_mouse_position = mouse_position;
    }

    void EditorCameraController::onMouseScrolled(const dodoe::MouseScrolledEvent& event) {
        m_scroll_delta_y += event.y_offset;
    }

    bool EditorCameraController::initialize(const EditorCameraControllerCreateInfo &info) {
        m_camera = info.camera;
        if (!m_camera) {
            return false;
        }
        m_last_mouse_position = dodoe::Input::GetMouseWindowPosition();
        dodoe::EventSystem::Subscribe<dodoe::MouseScrolledEvent, &EditorCameraController::onMouseScrolled>(this);

        return true;
    }

    void EditorCameraController::shutdown() {
        dodoe::EventSystem::Unsubscribe<dodoe::MouseScrolledEvent, &EditorCameraController::onMouseScrolled>(this);
    }

} // cakery
