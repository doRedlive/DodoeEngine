// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(CircleRendererComponent)

namespace dodoe {

    STRUCT(CircleRendererComponent, WhiteListFields) {
        REFLECTION_BODY(CircleRendererComponent)

        META(Enable)
        Float radius{1.0f};
        META(Enable)
        Color color{};
        META(Enable)
        UInt32 segments{32};
        META(Enable)
        Float thickness{0.0f};

        Bool dirty{false};

        CircleRendererComponent() = default;
    };

} // dodoe
