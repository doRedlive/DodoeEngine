// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

REFLECTION_TYPE(DistanceJoint2dComponent)
REFLECTION_TYPE(RevoluteJoint2dComponent)

namespace dodoe {

    STRUCT(DistanceJoint2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(DistanceJoint2dComponent)

        META(Enable)
        UUID target_entity{};
        META(Enable)
        Vector2f local_anchor_a{ 0.0f, 0.0f };
        META(Enable)
        Vector2f local_anchor_b{ 0.0f, 0.0f };
        META(Enable)
        float length{ 1.0f };
        META(Enable)
        float frequency{ 0.0f };
        META(Enable)
        float damping_ratio{ 0.0f };

        DistanceJoint2dComponent() = default;
    };

    STRUCT(RevoluteJoint2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(RevoluteJoint2dComponent)

        META(Enable)
        UUID target_entity{};
        META(Enable)
        Vector2f local_anchor_a{ 0.0f, 0.0f };
        META(Enable)
        Vector2f local_anchor_b{ 0.0f, 0.0f };
        META(Enable)
        bool enable_limit{ false };
        META(Enable)
        float lower_angle{ 0.0f };
        META(Enable)
        float upper_angle{ 0.0f };
        META(Enable)
        bool enable_motor{ false };
        META(Enable)
        float motor_speed{ 0.0f };
        META(Enable)
        float max_motor_torque{ 0.0f };

        RevoluteJoint2dComponent() = default;
    };

} // dodoe
