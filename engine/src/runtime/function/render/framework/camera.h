//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_CAMERA_H
#define DODOE_CAMERA_H

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    enum class CameraType {
        None = 0,
        Perspective,
        Orthographic,
    };

    struct CameraCreateInfo {
        CameraType camera_type{CameraType::Orthographic};
        Vector2f logical_size{};
        Vector2f window_size{};
        float vertical_fov_radians{Math::radians(60.0f)};
        float near_plane{0.01f};
        float far_plane{1000.0f};
    };

    class Camera {
    public:
        static Scope<Camera> create(CameraCreateInfo create_info);
        static void destroy(Scope<Camera>& camera);

        void translate(const Vector3f& offset);
        void zoom(float delta);
        void rotate(float delta);

        Vector3f world2screen(const Vector3f& world_pos) const;
        Vector3f screen2world(const Vector3f& screen_pos) const;

        void setCameraType(CameraType type);
        void setPosition(const Vector3f& position);
        void setZoom(float zoom);
        void setRotation(float rotation);
        void setClearColor(const Color& color);
        void setViewportSize(const Vector2f& logical_size, const Vector2f& window_size);
        void setLogicalSize(const Vector2f& logical_size);
        void setWindowSize(const Vector2f& window_size);
        void setPerspective(float vertical_fov_radians, float near_plane, float far_plane);
        void setOrthographic(float near_plane, float far_plane);

        [[nodiscard]] CameraType getCameraType() const;
        [[nodiscard]] const Vector3f& getPosition() const;
        [[nodiscard]] float getZoom() const;
        [[nodiscard]] float getRotation() const;
        [[nodiscard]] const Color& getClearColor() const;
    
        [[nodiscard]] const Vector2f& getLogicalSize() const;
        [[nodiscard]] const Vector2f& getWindowSize() const;
        [[nodiscard]] float getVerticalFovRadians() const;
        [[nodiscard]] float getNearPlane() const;
        [[nodiscard]] float getFarPlane() const;
        [[nodiscard]] const Matrix4f& getViewMatrix() const;
        [[nodiscard]] const Matrix4f& getProjectionMatrix() const;
        [[nodiscard]] Matrix4f getViewProjectionMatrix() const;

    private:
        CameraType m_camera_type{CameraType::Orthographic};

        Vector3f m_position{0.0f, 0.0f, 0.0f};
        float m_zoom{1.0f};
        float m_rotation{0.0f};
        Color m_background{Color::white()};
        float m_vertical_fov_radians{Math::radians(60.0f)};
        float m_near_plane{0.01f};
        float m_far_plane{1000.0f};

        Vector2f m_logical_size{};
        Vector2f m_window_size{};

        Matrix4f m_view_matrix{};
        Matrix4f m_projection_matrix{};

        void initialize(const CameraCreateInfo& create_info);
        void shutdown();

        void updateViewMatrix();
        void updateProjectionMatrix();
    };

} // dodoe

#endif//DODOE_CAMERA_H
