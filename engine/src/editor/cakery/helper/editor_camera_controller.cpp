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
        const auto make_forward = [this]() {
            return dodoe::Math::normalize(dodoe::Vector3f(
                std::cos(m_pitch) * std::cos(m_yaw),
                std::sin(m_pitch),
                std::cos(m_pitch) * std::sin(m_yaw)
            ));
        };
        const auto make_right = [](const dodoe::Vector3f& forward) {
            return dodoe::Math::normalize(dodoe::Math::cross(forward, dodoe::Vector3f(0.0f, 1.0f, 0.0f)));
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
            move_axis = dodoe::Math::normalize(move_axis);
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
                const float pitch_limit = dodoe::Math::radians(89.0f);
                m_pitch = dodoe::Math::clamp(m_pitch, -pitch_limit, pitch_limit);
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
        (void)info;
        m_camera = dodoe::Camera::create({dodoe::CameraType::Perspective});
        if (m_camera) {
            m_camera->setPosition(dodoe::Vector3f(0.0f, 0.0f, 5.0f));
            m_camera->setViewDirection(dodoe::Vector3f(0.0f, 0.0f, -1.0f));
        }
        m_last_mouse_position = dodoe::Input::GetMouseWindowPosition();
        dodoe::EventSystem::Subscribe<dodoe::MouseScrolledEvent, &EditorCameraController::onMouseScrolled>(this);

        return m_camera != nullptr;
    }

    void EditorCameraController::shutdown() {
        dodoe::EventSystem::Unsubscribe<dodoe::MouseScrolledEvent, &EditorCameraController::onMouseScrolled>(this);
        dodoe::Camera::destroy(m_camera);
    }

} // cakery
