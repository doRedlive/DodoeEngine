//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_COMPONENT
#define DODOE_CAMERA2D_COMPONENT

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(Camera2dComponent)

namespace dodoe {

    enum class CameraType {
        None = 0,
        Perspective,
        Orthographic,
    };

    STRUCT(Camera2dComponent, WhiteListFields) {
        REFLECTION_BODY(Camera2dComponent)

        META(Enable)
        CameraType type{CameraType::Orthographic};
        META(Enable)
        float zoom{1.0f};
        META(Enable)
        float fov{60.0f};
        META(Enable)
        float near_plane{0.01f};
        META(Enable)
        float far_plane{1000.0f};
        META(Enable)
        float aspect_ratio{16.0f / 9.0f};
        META(Enable)
        Color background{Color::white()};

        bool dirty{false};

        Matrix4f view_matrix{1.0f};
        Matrix4f projection_matrix{1.0f};

        void setCameraType(const CameraType in_type) { type = in_type; dirty = true; }
        void setZoom(const float in_zoom) { zoom = in_zoom; dirty = true; }
        void setBackgroundColor(const Color& in_background) { background = in_background; dirty = true; }
    };

} // dodoe

#endif//DODOE_CAMERA2D_COMPONENT
