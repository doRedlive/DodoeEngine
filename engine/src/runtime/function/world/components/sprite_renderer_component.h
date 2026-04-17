// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

REFLECTION_TYPE(SpriteRendererComponent)

namespace dodoe {

    STRUCT(SpriteRendererComponent, WhiteListFields) {
        REFLECTION_BODY(SpriteRendererComponent)

        META(Enable)
        identifier texture_id{ 0 };
        META(Enable)
        bool flip{ false };
        META(Enable)
        Vector2f pivot{0.0f, 0.0f};
        META(Enable)
        float depth_{0.0f};
        META(Enable)
        Color color{ };
        SpriteRendererComponent() = default;
    };

} // dodoe