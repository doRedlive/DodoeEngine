// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(BoxColliderComponent)

namespace dodoe {
    STRUCT(BoxColliderComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(BoxColliderComponent)

        META(Enable)
        Vector3f offset{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f size{ 1.0f, 1.0f, 1.0f };
        META(Enable)
        bool is_sensor{ false };
        META(Enable)
        uint32_t layer{ 1 };
        META(Enable)
        uint32_t mask{ 0xFFFFFFFF };
        META(Enable)
        float density{ 1.0f };
        META(Enable)
        float friction{ 0.5f };
        META(Enable)
        float restitution{ 0.0f };
    };

} // dodoe
