// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/common.h"

REFLECTION_TYPE(PointLightComponent)
REFLECTION_TYPE(SpotLightComponent)

namespace dodoe {

    STRUCT(PointLightComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(PointLightComponent)

        META(Enable)
        Color color{Color::white()};
        META(Enable)
        float intensity{1.0f};
        META(Enable)
        float radius{0.0f};
        META(Enable)
        float range{10.0f};

        bool enabled{true};
        bool dirty{true};
    };

    STRUCT(SpotLightComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(SpotLightComponent)

        META(Enable)
        Color color{Color::white()};
        META(Enable)
        float intensity{1.0f};
        META(Enable)
        float radius{0.0f};
        META(Enable)
        float range{10.0f};
        META(Enable)
        float inner_angle{30.0f};
        META(Enable)
        float outer_angle{45.0f};

        bool enabled{true};
        bool dirty{true};
    };

} // dodoe