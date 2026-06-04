// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/framework/texture.h"

REFLECTION_TYPE(SpriteRendererComponent)

namespace dodoe {

    STRUCT(SpriteRendererComponent, WhiteListFields) {
        REFLECTION_BODY(SpriteRendererComponent)

        META(Enable)
        PPtr<Texture> texture{};
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