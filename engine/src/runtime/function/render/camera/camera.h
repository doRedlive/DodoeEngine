//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_CAMERA_H
#define DODOE_CAMERA_H

#include "dopch.h"

namespace dodoe {

    enum class CameraType {
        None = 0,
        Perspective,
        Orthographic,
    };

    struct CameraCreateInfo {
        CameraType camera_type{CameraType::Orthographic};
        Vector2f logical_size{};
    };

    class Camera {
    public:
        static Scope<Camera> create(CameraCreateInfo create_info);
        static void destroy(Scope<Camera>& camera);

        void translate(const Vector3f& offset);
        void zoom(float delta);
        void rotate(float delta);

        Vector2f world2screen(const Vector2f& world_pos) const;
        Vector2f screen2world(const Vector2f& screen_pos) const;

        void set_position(const Vector3f& position);
        void set_zoom(float zoom);
        void set_rotation(float rotation);

        [[nodiscard]] const Vector3f& get_position() const;
        [[nodiscard]] float get_zoom() const;
        [[nodiscard]] float get_rotation() const;
    
        [[nodiscard]] const Matrix4f& view_matrix() const;
        [[nodiscard]] const Matrix4f& projection_matrix() const;
        [[nodiscard]] Matrix4f view_projection_matrix() const;

    private:
        CameraType type_{};

        Vector3f position_{0.0f, 0.0f, 0.0f};
        float zoom_{1.0f};
        float rotation_{0.0f};

        Matrix4f view_{};
        Matrix4f projection_{};

        void initialize(CameraCreateInfo create_info);
        void shutdown();

        void calc_view();
    };

} // dodoe

#endif//DODOE_CAMERA_H
