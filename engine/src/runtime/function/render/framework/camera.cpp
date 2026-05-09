//
// Created by Redlive on 2026/3/20.
//

#include "camera.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/render_api.h"

namespace dodoe {

    namespace {
        constexpr float kMinZoom = 0.001f;
        constexpr float kMinPerspectiveFov = 0.08726646f; // 5 deg
        constexpr float kMaxPerspectiveFov = 2.09439510f; // 120 deg
        constexpr float kDefaultPerspectiveNear = 0.01f;
        constexpr float kDefaultPerspectiveFar = 1000.0f;
        constexpr float kDefaultOrthographicNear = -1000.0f;
        constexpr float kDefaultOrthographicFar = 1000.0f;
    }

    Scope<Camera> Camera::create(const CameraCreateInfo& create_info) {
        auto context = create_scope<Camera>();
        context->initialize(create_info);
        return context;
    }

    void Camera::destroy(Scope<Camera>& camera) {
        if (!camera) {
            return;
        }

        camera->shutdown();
        camera.reset();
    }

    void Camera::translate(const Vector3f& offset) {
        setPosition(m_position + offset);
    }

    void Camera::zoom(const float delta) {
        setZoom(m_zoom + delta);
    }

    void Camera::rotate(const float delta) {
        setRotation(m_rotation + delta);
    }

    Vector3f Camera::world2screen(const Vector3f& world_pos) const {
        if (m_logical_size.x <= 0.0f || m_logical_size.y <= 0.0f) {
            return world_pos;
        }

        const Vector4f clip = getViewProjectionMatrix() * Vector4f(world_pos, 1.0f);
        if (clip.w == 0.0f) {
            return world_pos;
        }

        const Vector3f ndc = Vector3f(clip) / clip.w;
        return Vector3f(
            (ndc.x * 0.5f + 0.5f) * m_logical_size.x,
            (ndc.y * 0.5f + 0.5f) * m_logical_size.y,
            world_pos.z
        );
    }

    Vector3f Camera::screen2world(const Vector3f& screen_pos) const {
        if (m_logical_size.x <= 0.0f || m_logical_size.y <= 0.0f) {
            return screen_pos;
        }

        const float ndc_x = screen_pos.x / m_logical_size.x * 2.0f - 1.0f;
        const float ndc_y = screen_pos.y / m_logical_size.y * 2.0f - 1.0f;
        const Vector4f clip(ndc_x, ndc_y, 0.0f, 1.0f);
        const Matrix4f inv_vp = Math::inverse(getViewProjectionMatrix());
        const Vector4f world = inv_vp * clip;
        if (world.w == 0.0f) {
            return screen_pos;
        }

        return Vector3f(world) / world.w;
    }

    void Camera::setCameraType(const CameraType type) {
        m_camera_type = type;
        if (m_camera_type == CameraType::Orthographic && m_near_plane >= 0.0f) {
            m_near_plane = kDefaultOrthographicNear;
            m_far_plane = (std::max)(m_far_plane, kDefaultOrthographicFar);
        }
        if (m_camera_type == CameraType::Perspective && m_near_plane <= 0.0f) {
            m_near_plane = kDefaultPerspectiveNear;
            m_far_plane = (std::max)(m_far_plane, kDefaultPerspectiveFar);
        }
        updateProjectionMatrix();
    }

    void Camera::setPosition(const Vector3f& position) {
        m_position = position;
        updateViewMatrix();
    }

    void Camera::setZoom(const float zoom) {
        m_zoom = (std::max)(zoom, kMinZoom);
        updateProjectionMatrix();
    }

    void Camera::setRotation(const float rotation) {
        m_rotation = rotation;
        m_use_view_direction = false;
        updateViewMatrix();
    }

    void Camera::setViewDirection(const Vector3f& direction) {
        const float length = Math::length(direction);
        if (length <= Math::epsilon<float>()) {
            return;
        }
        m_view_direction = direction / length;
        m_use_view_direction = true;
        updateViewMatrix();
    }

    void Camera::clearViewDirection() {
        m_use_view_direction = false;
        updateViewMatrix();
    }

    void Camera::setClearColor(const Color& color) {
        m_background = color;
    }

    void Camera::setViewportSize(const Vector2f& logical_size, const Vector2f& window_size) {
        m_logical_size = logical_size;
        m_window_size = window_size;
        updateProjectionMatrix();
    }

    void Camera::setLogicalSize(const Vector2f& logical_size) {
        m_logical_size = logical_size;
        updateProjectionMatrix();
    }

    void Camera::setWindowSize(const Vector2f& window_size) {
        m_window_size = window_size;
        updateProjectionMatrix();
    }

    void Camera::setPerspective(const float vertical_fov_radians, const float near_plane, const float far_plane) {
        m_camera_type = CameraType::Perspective;
        m_vertical_fov_radians = vertical_fov_radians;
        m_near_plane = near_plane;
        m_far_plane = far_plane;
        updateProjectionMatrix();
    }

    void Camera::setOrthographic(const float near_plane, const float far_plane) {
        m_camera_type = CameraType::Orthographic;
        m_near_plane = near_plane;
        m_far_plane = far_plane;
        updateProjectionMatrix();
    }

    CameraType Camera::getCameraType() const {
        return m_camera_type;
    }

    const Vector3f& Camera::getPosition() const {
        return m_position;
    }

    float Camera::getZoom() const {
        return m_zoom;
    }

    float Camera::getRotation() const {
        return m_rotation;
    }

    const Vector3f& Camera::getViewDirection() const {
        return m_view_direction;
    }

    const Color& Camera::getClearColor() const {
        return m_background;
    }

    const Vector2f& Camera::getLogicalSize() const {
        return m_logical_size;
    }

    const Vector2f& Camera::getWindowSize() const {
        return m_window_size;
    }

    float Camera::getVerticalFovRadians() const {
        return m_vertical_fov_radians;
    }

    float Camera::getNearPlane() const {
        return m_near_plane;
    }

    float Camera::getFarPlane() const {
        return m_far_plane;
    }

    const Matrix4f& Camera::getViewMatrix() const {
        return m_view_matrix;
    }

    const Matrix4f& Camera::getProjectionMatrix() const {
        return m_projection_matrix;
    }

    Matrix4f Camera::getViewProjectionMatrix() const {
        return m_projection_matrix * m_view_matrix;
    }

    void Camera::initialize(const CameraCreateInfo& create_info) {
        m_camera_type = create_info.camera_type;
        m_logical_size = create_info.logical_size;
        m_window_size = create_info.window_size;
        m_vertical_fov_radians = create_info.vertical_fov_radians;
        m_near_plane = create_info.near_plane;
        m_far_plane = create_info.far_plane;

        if (m_camera_type == CameraType::Orthographic && m_near_plane >= 0.0f) {
            m_near_plane = kDefaultOrthographicNear;
            m_far_plane = (std::max)(m_far_plane, kDefaultOrthographicFar);
        }
        if (m_camera_type == CameraType::Perspective && m_near_plane <= 0.0f) {
            m_near_plane = kDefaultPerspectiveNear;
            m_far_plane = (std::max)(m_far_plane, kDefaultPerspectiveFar);
        }

        if (m_logical_size.x <= 0.0f || m_logical_size.y <= 0.0f) {
            DO_ERROR("Camera: logical size is invalid.");
            m_logical_size = Vector2f(1.0f, 1.0f);
        }

        if (m_window_size.x <= 0.0f || m_window_size.y <= 0.0f) {
            m_window_size = m_logical_size;
        }

        m_zoom = 1.0f;
        updateProjectionMatrix();
        updateViewMatrix();
    }

    void Camera::shutdown() {
    }

    void Camera::updateViewMatrix() {
        if (m_camera_type == CameraType::Perspective && m_use_view_direction) {
            Vector3f up(0.0f, 1.0f, 0.0f);
            if (std::abs(Math::dot(m_view_direction, up)) > 0.999f) {
                up = Vector3f(0.0f, 0.0f, 1.0f);
            }
            m_view_matrix = Math::lookAt(m_position, m_position + m_view_direction, up);
            return;
        }

        auto view = Matrix4f(1.0f);
        if (!Math::epsilonEqual(m_rotation, 0.0f, Math::epsilon<float>())) {
            view = Math::rotate(view, -m_rotation, Vector3f(0.0f, 0.0f, 1.0f));
        }

        m_view_matrix = Math::translate(view, Vector3f(-m_position.x, -m_position.y, -m_position.z));
    }

    void Camera::updateProjectionMatrix() {
        const float logical_width = m_logical_size.x > 0.0f ? m_logical_size.x : 1.0f;
        const float logical_height = m_logical_size.y > 0.0f ? m_logical_size.y : 1.0f;
        const float aspect = logical_width / logical_height;
        const float zoom = (std::max)(m_zoom, kMinZoom);
        const float near_plane = m_near_plane;
        const float far_plane = (std::max)(m_far_plane, near_plane + 0.001f);

        if (m_camera_type == CameraType::Perspective) {
            const float vertical_fov = Math::clamp(m_vertical_fov_radians / zoom, kMinPerspectiveFov, kMaxPerspectiveFov);
            m_projection_matrix = Math::perspective(vertical_fov, aspect, (std::max)(near_plane, 0.001f), far_plane);
        }
        else {
            const float half_width = logical_width * 0.5f / zoom;
            const float half_height = logical_height * 0.5f / zoom;
            m_projection_matrix = Math::ortho(-half_width, half_width, -half_height, half_height, near_plane, far_plane);
        }

        // Vulkan clip-space has inverted Y compared with OpenGL.
        if (RenderApi::apiType() == RenderApiType::Vulkan) {
            m_projection_matrix[1][1] *= -1.0f;
        }
    }

} // dodoe
