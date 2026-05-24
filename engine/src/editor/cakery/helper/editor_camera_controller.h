// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/event/event.h"
#include "runtime/function/render/framework/camera.h"

namespace cakery {

    struct EditorCameraControllerCreateInfo {

    };

    class EditorCameraController {
        dodoe::Scope<dodoe::Camera> m_camera{nullptr};

        float m_move_speed{5.0f};
        float m_rotate_speed{0.01f};
        float m_zoom_speed{0.1f};
        float m_fast_speed_multiplier{3.0f};
        float m_slow_speed_multiplier{0.35f};
        float m_yaw{-1.5707963f};
        float m_pitch{0.0f};

        dodoe::Vector2f m_last_mouse_position{0.0f, 0.0f};
        bool m_right_dragging{false};
        float m_scroll_delta_y{0.0f};

    public:
        static dodoe::Scope<EditorCameraController> Create(const EditorCameraControllerCreateInfo& info);
        static void Destroy(dodoe::Scope<EditorCameraController>& controller);

        void onUpdate(float dt);
        [[nodiscard]] dodoe::Camera* camera() const { return m_camera.get(); }

    private:
        void onMouseScrolled(const dodoe::MouseScrolledEvent& event);
        bool initialize(const EditorCameraControllerCreateInfo& info);
        void shutdown();
    };
    
} // cakery
