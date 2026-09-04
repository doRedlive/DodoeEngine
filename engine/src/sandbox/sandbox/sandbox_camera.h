// do@Redlive

#pragma once

#include "runtime/core/math/math.h"
#include "runtime/function/render/render_view/camera_provider.h"

namespace sandbox {

    class SandboxCamera {
    public:
        SandboxCamera();

        void setViewportSize(float w, float h);
        void update(float dt);

        [[nodiscard]] dodoe::Matrix4f view() const;
        [[nodiscard]] dodoe::Matrix4f projection() const;

        [[nodiscard]] dodoe::Vector3f position() const { return m_position; }
        void setPosition(const dodoe::Vector3f& pos) { m_position = pos; }

    private:
        [[nodiscard]] dodoe::Vector3f forward() const;
        [[nodiscard]] dodoe::Vector3f right() const;

        dodoe::Vector3f m_position{0.0f, 5.0f, 10.0f};
        float m_yaw   = -90.0f;
        float m_pitch = -20.0f;
        float m_fov   = 60.0f;

        float m_speed      = 10.0f;
        float m_look_speed = 0.15f;

        float m_vpW = 1280.0f;
        float m_vpH = 720.0f;

        static constexpr float kPitchLimit = 89.0f;
        static constexpr float kMinSpeed   = 1.0f;
        static constexpr float kMaxSpeed   = 100.0f;
    };

    class SandboxCameraProvider final : public dodoe::ICameraProvider {
    public:
        explicit SandboxCameraProvider(SandboxCamera* camera) : m_camera(camera) {}

        [[nodiscard]] dodoe::Matrix4f getView() const override {
            return m_camera ? m_camera->view() : dodoe::Matrix4f(1.0f);
        }

        [[nodiscard]] dodoe::Matrix4f getProj() const override {
            return m_camera ? m_camera->projection() : dodoe::Matrix4f(1.0f);
        }

    private:
        SandboxCamera* m_camera{nullptr};
    };

} // sandbox
