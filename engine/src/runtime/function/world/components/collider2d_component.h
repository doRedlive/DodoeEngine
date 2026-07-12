// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(BoxCollider2dComponent)

namespace dodoe {

    STRUCT(BoxCollider2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(BoxCollider2dComponent)

        META(Enable)
        Vector2f offset{ 0.0f,0.0f };
        META(Enable)
        Vector2f size{ 10.0f, 10.0f };

        META(Enable)
        float density{ 1.0f };
        META(Enable)
        float friction{ 0.5f };
        META(Enable)
        float restitution{ 0.0f };
        META(Enable)
        float restitution_threshold{ 0.5f };

        BoxCollider2dComponent() = default;
    };

} // dodoe