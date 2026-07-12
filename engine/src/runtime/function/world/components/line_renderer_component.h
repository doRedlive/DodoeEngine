// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(LineRendererComponent)

namespace dodoe {

    STRUCT(LineRendererComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(LineRendererComponent)

        META(Enable)
        Vector2f direction{1.0f, 0.0f};
        META(Enable)
        Float length{1.0f};
        META(Enable)
        Float thickness{1.0f};
        META(Enable)
        Color color{};

        Bool dirty{false};

        LineRendererComponent() = default;
    };

} // dodoe
