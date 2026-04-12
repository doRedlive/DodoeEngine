// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    struct SpriteRendererComponent {
        identifier texture_id{ 0 };
        bool flip{ false };
        Vector2f pivot{0.0f, 0.0f};
        float depth_{0.0f};
        Color color{ };
        SpriteRendererComponent() = default;
    };

} // dodoe