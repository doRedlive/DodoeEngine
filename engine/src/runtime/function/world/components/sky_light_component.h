// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(SkyLightComponent)

namespace dodoe {

    STRUCT(SkyLightComponent, WhiteListFields) {
        REFLECTION_BODY(SkyLightComponent)

        META(Enable)
        DynamicArray<String> face_paths{};
        META(Enable)
        Float intensity{1.0f};

        Bool enabled{true};
        Bool dirty{true};
    };

} // dodoe
