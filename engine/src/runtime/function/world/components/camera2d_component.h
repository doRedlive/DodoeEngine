//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_COMPONENT
#define DODOE_CAMERA2D_COMPONENT

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/render/framework/camera.h"

REFLECTION_TYPE(Camera2dComponent)

namespace dodoe {

    STRUCT(Camera2dComponent, WhiteListFields) {
        REFLECTION_BODY(Camera2dComponent)

        META(Enable)
        CameraType type{CameraType::Orthographic};
        META(Enable)
        float zoom{1.0f};
        META(Enable)
        Color background{Color::white()};
        
        bool dirty{false};

        void setCameraType(CameraType in_type) { type = in_type;  dirty = true; }
        void setZoom(float in_zoom) { zoom = in_zoom; dirty = true; }
        void setBackgroundColor(const Color& in_background) { background = in_background; dirty = true; }
        [[nodiscard]] CameraType getCameraType() { return type; }
        [[nodiscard]] float getZoom() { return zoom; }
        [[nodiscard]] const Color& getBackgroundColor() { return background; }
    };

} // dodoe

#endif//DODOE_CAMERA2D_COMPONENT
