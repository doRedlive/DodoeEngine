//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_CAMERA_H
#define DODOE_CAMERA_H

#include "dopch.h"

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

        void set_position(const Vector3f& position);
        void set_zoom(float zoom);
        void set_rotation(float rotation);
        void set_clear_color(const Color& color);

        [[nodiscard]] const Vector3f& get_position() const;
        [[nodiscard]] float get_zoom() const;
        [[nodiscard]] float get_rotation() const;
        [[nodiscard]] const Color& get_clear_color() const;
    
        [[nodiscard]] const Matrix4f& view_matrix() const;
        [[nodiscard]] const Matrix4f& projection_matrix() const;
        [[nodiscard]] Matrix4f view_projection_matrix() const;

    private:
        CameraType type_{};

        Vector3f position_{0.0f, 0.0f, 0.0f};
        float zoom_{1.0f};
        float rotation_{0.0f};
        Color background_{Color::white()};

        Vector2f logical_size_{};
        Vector2f window_size_{};

        Matrix4f view_{};
        Matrix4f projection_{};

        void initialize(CameraCreateInfo create_info);
        void shutdown();

        void calc_view();
    };

} // dodoe

#endif//DODOE_CAMERA_H
