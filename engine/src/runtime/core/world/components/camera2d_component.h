//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_COMPONENT
#define DODOE_CAMERA2D_COMPONENT

#include "dopch.h"

#include "runtime/core/utils/util.h"

#include "runtime/function/render/camera/camera.h"


namespace dodoe {

    namespace component {

        struct Camera2dComponent {
            CameraType type{CameraType::Orthographic};
            float zoom{1.0f};
            Color background{Color::white()};
            
            bool dirty{false};

            void set_camera_type(CameraType in_type) { type = in_type;  dirty = true; }
            void set_zoom(float in_zoom) { zoom = in_zoom; dirty = true; }
            void set_background_color(const Color& in_background) { background = in_background; dirty = true; }
            [[nodiscard]] CameraType get_camera_type() { return type; }
            [[nodiscard]] float get_zoom() { return zoom; }
            [[nodiscard]] const Color& get_background_color() { return background; }
        };

    } // component 

} // dodoe

#endif//DODOE_CAMERA2D_COMPONENT