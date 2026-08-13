// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(Rigidbody2dComponent)

namespace dodoe {
    STRUCT(Rigidbody2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(Rigidbody2dComponent)

        enum class BodyType {
            Static = 0,
            Dynamic = 1,
            Kinematic = 2
        };

        META(Enable)
        BodyType type{ BodyType::Static };
        META(Enable)
        float gravity_scale{1.0f};
        META(Enable)
        bool fixed_rotation{ false };

        Vector2f velocity_request{ 0.0f, 0.0f };
        Vector2f force_request{ 0.0f, 0.0f };
        Vector2f impulse_request{ 0.0f, 0.0f };
    };

} // dodoe
