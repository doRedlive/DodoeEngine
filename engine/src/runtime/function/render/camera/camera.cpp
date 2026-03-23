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

    const Vector3f& Camera::get_position() const {
        return position_;
    }

    float Camera::get_zoom() const {
        return zoom_;
    }

    float Camera::get_rotation() const {
        return rotation_;
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
