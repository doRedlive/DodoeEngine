// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(BoxCollider2dComponent)
REFLECTION_TYPE(CircleCollider2dComponent)

namespace dodoe {

    STRUCT(BoxCollider2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(BoxCollider2dComponent)

        META(Enable)
        Vector2f offset{ 0.0f,0.0f };
        META(Enable)
        Vector2f size{ 10.0f, 10.0f };

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
        META(Enable)
        float restitution_threshold{ 0.5f };

        BoxCollider2dComponent() = default;
    };

    STRUCT(CircleCollider2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(CircleCollider2dComponent)

        META(Enable)
        Vector2f offset{ 0.0f, 0.0f };
        META(Enable)
        float radius{ 5.0f };

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
        META(Enable)
        float restitution_threshold{ 0.5f };

        CircleCollider2dComponent() = default;
    };

} // dodoe
