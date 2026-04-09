//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_PHYSICS2D_SYSTEM_H
#define DODOE_PHYSICS2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/components.h"
#include "runtime/function/physics/physics_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

namespace dodoe {

    namespace system {

        class Physics2dSystem : public System {
        public:
            ~Physics2dSystem() override = default;

            void start(Registry& reg) override {
                auto world_id = Application::self().context().physics_system->world_id_;
                if (!B2_IS_NON_NULL(world_id)) {
                    return;
                }
                // MARK: TODO: Make physics material.
                auto view = reg.view<Rigidbody2dComponent, TransformComponent>();
                for (auto entity : view) {
                    const ui32 key = static_cast<ui32>(entity);
                    if (body_umap_.contains(key)) {
                        continue;
                    }

                    auto& transform = reg.get<TransformComponent>(entity);
                    auto& rb2d = reg.get<Rigidbody2dComponent>(entity);

                    b2BodyDef body_def = b2DefaultBodyDef();
                    body_def.type = rigidbody2d_type2box2d_type_(rb2d.type);
                    body_def.position = { transform.position.x, transform.position.y };
                    body_def.rotation = b2MakeRot(transform.rotation.z);
                    body_def.gravityScale = rb2d.gravity_scale;

                    b2BodyId body_id = b2CreateBody(world_id, &body_def);
                    if (!B2_IS_NON_NULL(body_id)) {
                        DoError("Created b2body failed!");
                        continue;
                    }

                    b2Body_SetFixedRotation(body_id, rb2d.fixed_rotation);
                    body_umap_[key] = body_id;
                    rb2d.body_id = body_id;

                    if (reg.all_of<BoxCollider2dComponent>(entity)) {
                        auto& bc2d = reg.get<BoxCollider2dComponent>(entity);
                        b2ShapeDef shape_def = b2DefaultShapeDef();
                        shape_def.density = bc2d.density;
                        shape_def.material.friction = bc2d.friction;
                        shape_def.material.restitution = bc2d.restitution;

                        const float half_width = std::abs(bc2d.size.x * transform.scale.x);
                        const float half_height = std::abs(bc2d.size.y * transform.scale.y);
                        if (half_width <= FLT_EPSILON || half_height <= FLT_EPSILON) {
                            continue;
                        }
                        b2Polygon box = b2MakeOffsetBox(half_width, half_height, {bc2d.offset.x, bc2d.offset.y}, b2Rot_identity);
                        b2ShapeId shape_id = b2CreatePolygonShape(body_id, &shape_def, &box);
                        if (B2_IS_NON_NULL(shape_id)) {
                            shape_umap_[key] = shape_id;
                        }
                    }
                }
            }

            void update(Registry& reg) override {
                auto view = reg.view<Rigidbody2dComponent, TransformComponent>();
                for (auto entity : view) {
                    auto& transform = reg.get<TransformComponent>(entity);
                    b2BodyId body_id = b2_nullBodyId;

                    const ui32 key = static_cast<ui32>(entity);
                    if (auto it = body_umap_.find(key); it != body_umap_.end()) {
                        body_id = it->second;
                    }

                    if (!B2_IS_NON_NULL(body_id)) {
                        continue;
                    }

                    const b2Vec2 position = b2Body_GetPosition(body_id);
                    const b2Rot rotation = b2Body_GetRotation(body_id);
                    transform.position.x = position.x;
                    transform.position.y = position.y;
                    transform.rotation.z = b2Rot_GetAngle(rotation);
                }
            }

            void finalize(Registry& reg) override {
                auto world_id = Application::self().context().physics_system->world_id_;
                if (B2_IS_NON_NULL(world_id)) {
                    for (auto& [_, body_id] : body_umap_) {
                        if (!B2_IS_NON_NULL(body_id)) {
                            continue;
                        }
                        b2DestroyBody(body_id);
                    }
                }
                body_umap_.clear();
                shape_umap_.clear();
            }

        private:
            static b2BodyType rigidbody2d_type2box2d_type_(Rigidbody2dComponent::BodyType type) {
                switch (type) {
                    case Rigidbody2dComponent::BodyType::Static: return b2_staticBody;
                    case Rigidbody2dComponent::BodyType::Dynamic: return b2_dynamicBody;
                    case Rigidbody2dComponent::BodyType::Kinematic: return b2_kinematicBody;
                    default: return b2_staticBody;
                }
            }

            std::unordered_map<ui32, b2BodyId> body_umap_{};
            std::unordered_map<ui32, b2ShapeId> shape_umap_{};

        public:
            b2BodyId get_body_id(const Entity& entity) const {
                const ui32 key = static_cast<ui32>(entity);
                if (auto it = body_umap_.find(key); it != body_umap_.end()) {
                    return it->second;
                }
                return b2_nullBodyId;
            }
        };

    } // system

} // dodoe

#endif//DODOE_PHYSICS2D_SYSTEM_H
