#include "physics2d_system.h"

namespace dodoe {

    Physics2dSystem::~Physics2dSystem() = default;

    void Physics2dSystem::start(Registry& reg) {
        auto world_id = GetPhysicsSystem()->world_id_;
        if (!B2_IS_NON_NULL(world_id)) {
            return;
        }

        auto& state = state_(reg);
        auto view = reg.view<Rigidbody2dComponent, TransformComponent>();
        for (auto entity : view) {
            const ui32 key = static_cast<ui32>(entity);
            if (state.body_umap.contains(key)) {
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
                DO_ERROR("Created b2body failed!");
                continue;
            }

            b2Body_SetFixedRotation(body_id, rb2d.fixed_rotation);
            state.body_umap[key] = body_id;
            rb2d.body_id = body_id;

            if (!reg.all_of<BoxCollider2dComponent>(entity)) {
                continue;
            }

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

            b2Polygon box = b2MakeOffsetBox(half_width, half_height, { bc2d.offset.x, bc2d.offset.y }, b2Rot_identity);
            b2ShapeId shape_id = b2CreatePolygonShape(body_id, &shape_def, &box);
            if (B2_IS_NON_NULL(shape_id)) {
                state.shape_umap[key] = shape_id;
            }
        }
    }

    void Physics2dSystem::update(Registry& reg, float dt) {
        if (dt > 0.0f) {
            auto* physics_system = GetPhysicsSystem();
            if (physics_system) {
                physics_system->step(dt);
            }
        }

        auto& state = state_(reg);
        auto view = reg.view<Rigidbody2dComponent, TransformComponent>();
        for (auto entity : view) {
            auto& transform = reg.get<TransformComponent>(entity);
            b2BodyId body_id = b2_nullBodyId;

            const ui32 key = static_cast<ui32>(entity);
            if (auto it = state.body_umap.find(key); it != state.body_umap.end()) {
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

    void Physics2dSystem::finalize(Registry& reg) {
        const auto it = registry_state_umap_.find(&reg);
        if (it == registry_state_umap_.end()) {
            return;
        }

        auto world_id = GetPhysicsSystem()->world_id_;
        if (B2_IS_NON_NULL(world_id)) {
            for (auto& [_, body_id] : it->second.body_umap) {
                if (B2_IS_NON_NULL(body_id)) {
                    b2DestroyBody(body_id);
                }
            }
        }
        registry_state_umap_.erase(it);
    }

    b2BodyId Physics2dSystem::get_body_id(Registry& reg, const Entity& entity) const {
        const auto it = registry_state_umap_.find(&reg);
        if (it == registry_state_umap_.end()) {
            return b2_nullBodyId;
        }

        const ui32 key = static_cast<ui32>(entity);
        if (auto body_it = it->second.body_umap.find(key); body_it != it->second.body_umap.end()) {
            return body_it->second;
        }
        return b2_nullBodyId;
    }

    Physics2dSystem::RegistryState& Physics2dSystem::state_(Registry& reg) {
        return registry_state_umap_[&reg];
    }

    b2BodyType Physics2dSystem::rigidbody2d_type2box2d_type_(Rigidbody2dComponent::BodyType type) {
        switch (type) {
            case Rigidbody2dComponent::BodyType::Static: return b2_staticBody;
            case Rigidbody2dComponent::BodyType::Dynamic: return b2_dynamicBody;
            case Rigidbody2dComponent::BodyType::Kinematic: return b2_kinematicBody;
            default: return b2_staticBody;
        }
    }

} // dodoe
