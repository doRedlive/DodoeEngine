// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    struct SetVelocity2dRequest {
        Vector2f velocity{ 0.0f, 0.0f };
    };

    struct ApplyForce2dRequest {
        Vector2f force{ 0.0f, 0.0f };
    };

    struct ApplyImpulse2dRequest {
        Vector2f impulse{ 0.0f, 0.0f };
    };

    struct SetVelocityRequest {
        Vector3f velocity{ 0.0f, 0.0f, 0.0f };
    };

    struct ApplyForceRequest {
        Vector3f force{ 0.0f, 0.0f, 0.0f };
    };

    struct ApplyImpulseRequest {
        Vector3f impulse{ 0.0f, 0.0f, 0.0f };
    };

    struct TeleportRequest {
        Vector3f position{ 0.0f, 0.0f, 0.0f };
        Quaternion rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    struct PlayAnimationRequest {
        String name;
    };

    struct StopAnimationRequest {
    };

    struct ResumeAnimationRequest {
    };

} // dodoe
