// do@Redlive

#include "sandbox_camera.h"

#include "runtime/core/context/system_context.h"

#include <algorithm>
#include <cmath>

namespace sandbox {

    SandboxCamera::SandboxCamera() = default;

    void SandboxCamera::setViewportSize(const float w, const float h) {
        m_vpW = w;
        m_vpH = h;
    }

    void SandboxCamera::update(const float dt) {
        auto* input = dodoe::GetInputManager();
        if (!input) {
            return;
        }

        if (input->isActionDown("Sandbox/Look")) {
            const dodoe::Vector2f delta = input->getMouseDelta();
            m_yaw += delta.x * m_look_speed;
            m_pitch -= delta.y * m_look_speed;
            m_pitch = std::clamp(m_pitch, -kPitchLimit, kPitchLimit);
        }

        const dodoe::Vector2f move = input->getActionVector2("Sandbox/Move");
        if (move.x != 0.0f || move.y != 0.0f) {
            m_position += forward() * (move.y * m_speed * dt);
            m_position += right() * (move.x * m_speed * dt);
        }

        if (input->isActionDown("Sandbox/Up")) {
            m_position.y += m_speed * dt;
        }
        if (input->isActionDown("Sandbox/Down")) {
            m_position.y -= m_speed * dt;
        }

        const dodoe::Vector2f wheel = input->getMouseWheel();
        if (wheel.y != 0.0f) {
            m_speed = std::clamp(m_speed + wheel.y * 2.0f, kMinSpeed, kMaxSpeed);
        }
    }

    dodoe::Vector3f SandboxCamera::forward() const {
        const float pitch_rad = glm::radians(m_pitch);
        const float yaw_rad   = glm::radians(m_yaw);
        return {
            std::cos(pitch_rad) * std::cos(yaw_rad),
            std::sin(pitch_rad),
            std::cos(pitch_rad) * std::sin(yaw_rad)
        };
    }

    dodoe::Vector3f SandboxCamera::right() const {
        const float yaw_rad = glm::radians(m_yaw);
        return {
            -std::sin(yaw_rad),
            0.0f,
            std::cos(yaw_rad)
        };
    }

    dodoe::Matrix4f SandboxCamera::view() const {
        return glm::lookAt(m_position, m_position + forward(), dodoe::Vector3f{0.0f, 1.0f, 0.0f});
    }

    dodoe::Matrix4f SandboxCamera::projection() const {
        const float aspect = (m_vpH > 0.0f) ? (m_vpW / m_vpH) : 1.0f;
        auto proj = glm::perspective(glm::radians(m_fov), aspect, 0.1f, 10000.0f);
        return dodoe::Math::FlipClipSpaceY(proj);
    }

} // namespace sandbox
