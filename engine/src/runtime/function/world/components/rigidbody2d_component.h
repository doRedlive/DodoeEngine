//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_RIGIDBODY2D_COMPONENT_H
#define DODOE_RIGIDBODY2D_COMPONENT_H

#include "dopch.h"

#include "box2d/box2d.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/math/math.h"

REFLECTION_TYPE(Rigidbody2dComponent)

namespace dodoe {
    STRUCT(Rigidbody2dComponent, WhiteListFields) {
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
        b2BodyId body_id{ b2_nullBodyId };

        void setLinearVelocity(const Vector2f& velocity) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_SetLinearVelocity(body_id, {velocity.x, velocity.y});
            }
        }

        void applyForceToCenter(const Vector2f& force, bool wake) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_ApplyForceToCenter(body_id, {force.x, force.y}, wake);
            }
        }

        void applyLinearImpulseToCenter(const Vector2f& impulse, bool wake) {
            if (B2_IS_NON_NULL(body_id)) {
                b2Body_ApplyLinearImpulseToCenter(body_id, {impulse.x, impulse.y}, wake);
            }
        }
    };

} // dodoe

#endif//DODOE_RIGIDBODY2D_COMPONENT_H