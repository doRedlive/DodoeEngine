//
// Created by Redlive on 2026/3/20.
//

#include "camera.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    Scope<Camera> Camera::create(CameraCreateInfo create_info) {
        auto context = create_scope<Camera>();
        context->initialize(create_info);
        return context;
    }
    
    void Camera::destroy(Scope<Camera>& camera) {
        if (!camera) return;
        camera->shutdown();
        camera.reset();
    }

    void Camera::translate(const Vector3f& offset) {
        set_position(position_ + offset);
    }

    void Camera::zoom(const float delta) {
        set_zoom(zoom_ + delta);
    }

    void Camera::rotate(const float delta) {
        set_rotation(rotation_ + delta);
    }

    Vector3f Camera::world2screen(const Vector3f& world_pos) const {
        if (logical_size_.x <= 0.0f || logical_size_.y <= 0.0f) {
            return world_pos;
        }

        const Vector4f clip = view_projection_matrix() * Vector4f(world_pos, 1.0f);
        if (clip.w == 0.0f) {
            return world_pos;
        }
        const Vector3f ndc = Vector3f(clip) / clip.w;
        return Vector3f(
            (ndc.x * 0.5f + 0.5f) * logical_size_.x,
            (ndc.y * 0.5f + 0.5f) * logical_size_.y,
            world_pos.z
        );
    }

    Vector3f Camera::screen2world(const Vector3f& screen_pos) const {
        if (logical_size_.x <= 0.0f || logical_size_.y <= 0.0f) {
            return screen_pos;
        }

        const float ndc_x = screen_pos.x / logical_size_.x * 2.0f - 1.0f;
        const float ndc_y = screen_pos.y / logical_size_.y * 2.0f - 1.0f;
        const Vector4f clip(ndc_x, ndc_y, 0.0f, 1.0f);
        const Matrix4f inv_vp = glm::inverse(view_projection_matrix());
        const Vector4f world = inv_vp * clip;
        if (world.w == 0.0f) {
            return screen_pos;
        }
        return Vector3f(world) / world.w;
    }

    void Camera::set_position(const Vector3f& position) {
        position_ = position;
        calc_view();
    }

    void Camera::set_zoom(const float zoom) {
        zoom_ = zoom;
        calc_view();
    }

    void Camera::set_rotation(const float rotation) {
        rotation_ = rotation;
        calc_view();
    }

    void Camera::set_clear_color(const Color& color) {
        background_ = color;
    }

    const Vector3f& Camera::get_position() const {
        return position_;
    }

    float Camera::get_zoom() const {
        return zoom_;
    }

    float Camera::get_rotation() const {
        return rotation_;
    }

    const Color& Camera::get_clear_color() const {
        return background_;
    }

    const Matrix4f& Camera::view_matrix() const {
        return view_;
    }

    const Matrix4f& Camera::projection_matrix() const {
        return projection_;
    }

    Matrix4f Camera::view_projection_matrix() const {
        return projection_ * view_;
    }

    void Camera::initialize(CameraCreateInfo create_info) {
        Vector2f size = create_info.logical_size;
        if (size.x <= 0.0f || size.y <= 0.0f) {
            DoError("The logical size is 0!");
            size = Vector2f(1.0f);
        }
        // MARK: TODO: when the camera type is orthographic:
        logical_size_ = size;
        window_size_ = create_info.window_size;
        projection_ = Math::ortho(-size.x * 0.5f, size.x * 0.5f, -size.y * 0.5f, size.y * 0.5f, -1.0f, 1.0f);
        calc_view();
    }

    void Camera::shutdown() {

    }

    void Camera::calc_view() {
        Matrix4f view = Matrix4f(1.0f);
        view = Math::scale(view, Vector3f(zoom_, zoom_, 1.0f));
        if (!Math::epsilonEqual(rotation_, 0.0f, Math::epsilon<float>())) {
            view = Math::rotate(view, -rotation_, Vector3f(0.0f, 0.0f, 1.0f));
        }

        view_ = Math::translate(view, Vector3f(-position_.x, -position_.y, 0.0f));
    }

} // dodoe
