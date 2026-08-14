// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(CapsuleColliderComponent)

namespace dodoe {
    STRUCT(CapsuleColliderComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(CapsuleColliderComponent)

        META(Enable)
        Vector3f offset{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        float radius{ 0.5f };
        META(Enable)
        float half_height{ 0.5f };
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
