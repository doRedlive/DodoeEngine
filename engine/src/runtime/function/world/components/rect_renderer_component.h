// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(RectRendererComponent)

namespace dodoe {

    STRUCT(RectRendererComponent, WhiteListFields) {
        REFLECTION_BODY(RectRendererComponent)

        META(Enable)
        Vector2f size{1.0f, 1.0f};
        META(Enable)
        Color color{};
        META(Enable)
        Float thickness{0.0f};

        Bool dirty{false};

        RectRendererComponent() = default;
    };

} // dodoe
