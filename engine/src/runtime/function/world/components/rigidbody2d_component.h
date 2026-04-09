//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_RIGIDBODY2D_COMPONENT_H
#define DODOE_RIGIDBODY2D_COMPONENT_H

#include "dopch.h"

#include "box2d/box2d.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    namespace component {

        struct Rigidbody2dComponent {
            enum class BodyType { Static = 0, Dynamic, Kinematic };
            BodyType type{ BodyType::Static };
            bool fixed_rotation{ false };
            float gravity_scale{ 1.0f };
            b2BodyId body_id{ b2_nullBodyId };

            Rigidbody2dComponent() = default;

            void set_linear_velocity(const Vector2f& velocity) {
                if (B2_IS_NON_NULL(body_id)) {
                    b2Body_SetLinearVelocity(body_id, {velocity.x, velocity.y});
                }
            }

            void apply_force_to_center(const Vector2f& force, bool wake) {
                if (B2_IS_NON_NULL(body_id)) {
                    b2Body_ApplyForceToCenter(body_id, {force.x, force.y}, wake);
                }
            }

            void apply_linear_impulse_to_center(const Vector2f& impulse, bool wake) {
                if (B2_IS_NON_NULL(body_id)) {
                    b2Body_ApplyLinearImpulseToCenter(body_id, {impulse.x, impulse.y}, wake);
                }
            }
        };

    } // component

} // dodoe

#endif//DODOE_RIGIDBODY2D_COMPONENT_H