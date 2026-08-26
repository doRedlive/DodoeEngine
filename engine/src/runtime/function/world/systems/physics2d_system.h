// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/physics/physics_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

namespace dodoe {

    class Scene;

    struct Collision2dEvent {
        ui32 entity_a{0};
        ui32 entity_b{0};
        Vector2f point{};
        Vector2f normal{};
        float relative_speed{0.0f};
        bool is_sensor{false};
        Contact2dPhase phase{Contact2dPhase::Begin};
    };

    struct RaycastHit2d {
        ui32 entity{0};
        Vector2f point{};
        Vector2f normal{};
        float fraction{0.0f};
    };

    struct Body2dSnapshot {
        Rigidbody2dComponent::BodyType type{};
        float gravity_scale{};
        bool fixed_rotation{};
        bool operator==(const Body2dSnapshot& other) const {
            return type == other.type
                && gravity_scale == other.gravity_scale
                && fixed_rotation == other.fixed_rotation;
        }
    };

    struct BoxShape2dSnapshot {
        Vector2f offset{};
        Vector2f size{};
        float density{};
        float friction{};
        float restitution{};
        float restitution_threshold{};
        bool is_sensor{};
        ui32 layer{};
        ui32 mask{};
        bool operator==(const BoxShape2dSnapshot& other) const {
            return offset == other.offset
                && size == other.size
                && density == other.density
                && friction == other.friction
                && restitution == other.restitution
                && restitution_threshold == other.restitution_threshold
                && is_sensor == other.is_sensor
                && layer == other.layer
                && mask == other.mask;
        }
    };

    struct CircleShape2dSnapshot {
        Vector2f offset{};
        float radius{};
        float density{};
        float friction{};
        float restitution{};
        float restitution_threshold{};
        bool is_sensor{};
        ui32 layer{};
        ui32 mask{};
        bool operator==(const CircleShape2dSnapshot& other) const {
            return offset == other.offset
                && radius == other.radius
                && density == other.density
                && friction == other.friction
                && restitution == other.restitution
                && restitution_threshold == other.restitution_threshold
                && is_sensor == other.is_sensor
                && layer == other.layer
                && mask == other.mask;
        }
    };

    struct DistanceJoint2dSnapshot {
        UUID target_entity{};
        Vector2f local_anchor_a{};
        Vector2f local_anchor_b{};
        float length{};
        float frequency{};
        float damping_ratio{};
        bool operator==(const DistanceJoint2dSnapshot& other) const {
            return target_entity == other.target_entity
                && local_anchor_a == other.local_anchor_a
                && local_anchor_b == other.local_anchor_b
                && length == other.length
                && frequency == other.frequency
                && damping_ratio == other.damping_ratio;
        }
    };

    struct RevoluteJoint2dSnapshot {
        UUID target_entity{};
        Vector2f local_anchor_a{};
        Vector2f local_anchor_b{};
        bool enable_limit{};
        float lower_angle{};
        float upper_angle{};
        bool enable_motor{};
        float motor_speed{};
        float max_motor_torque{};
        bool operator==(const RevoluteJoint2dSnapshot& other) const {
            return target_entity == other.target_entity
                && local_anchor_a == other.local_anchor_a
                && local_anchor_b == other.local_anchor_b
                && enable_limit == other.enable_limit
                && lower_angle == other.lower_angle
                && upper_angle == other.upper_angle
                && enable_motor == other.enable_motor
                && motor_speed == other.motor_speed
                && max_motor_torque == other.max_motor_torque;
        }
    };

    class Physics2dSystem : public System {
    public:
        ~Physics2dSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void start(Registry& reg) override;
        void update(Registry& reg, float dt) override;
        void finalize(Registry& reg) override;

        [[nodiscard]] b2BodyId getBodyId(Registry& reg, const Entity& entity) const;
        [[nodiscard]] Vector2f getBodyPosition(Registry& reg, const Entity& entity) const;
        [[nodiscard]] Vector2f getBodyLinearVelocity(Registry& reg, const Entity& entity) const;
        void moveBodyPosition(Registry& reg, const Entity& entity, const Vector2f& position);
        void takeCollisionEvents(Registry& reg, DynamicArray<Collision2dEvent>& out_events);
        void raycast(Registry& reg, const Vector2f& origin, const Vector2f& direction, float max_distance,
                     const Query2dFilter& filter, DynamicArray<RaycastHit2d>& out_hits) const;
        void boxCast(Registry& reg, const Vector2f& center, const Vector2f& half_size, float angle,
                     const Vector2f& direction, float max_distance,
                     const Query2dFilter& filter, DynamicArray<RaycastHit2d>& out_hits) const;
        void overlapAABB(Registry& reg, const Vector2f& center, const Vector2f& half_size,
                         const Query2dFilter& filter, DynamicArray<ui32>& out_entities) const;
        void ignoreCollision(Registry& reg, const Entity& a, const Entity& b, bool ignore);
        void getColliderContacts(Registry& reg, const Entity& entity, DynamicArray<ui32>& out_entities) const;
        bool colliderDistance(Registry& reg, const Entity& a, const Entity& b, float& out_distance) const;

    private:
        struct RegistryState {
            Scene* scene_context{ nullptr };
            UnorderedMap<ui32, b2BodyId> body_umap{};
            UnorderedMap<ui32, b2ShapeId> box_shape_umap{};
            UnorderedMap<ui32, b2ShapeId> circle_shape_umap{};
            UnorderedMap<ui32, b2JointId> distance_joint_umap{};
            UnorderedMap<ui32, b2JointId> revolute_joint_umap{};
            UnorderedMap<ui32, Vector2f> scale_umap{};
            UnorderedMap<ui32, Body2dSnapshot> body_snapshot_umap{};
            UnorderedMap<ui32, BoxShape2dSnapshot> box_snapshot_umap{};
            UnorderedMap<ui32, CircleShape2dSnapshot> circle_snapshot_umap{};
            UnorderedMap<ui32, DistanceJoint2dSnapshot> distance_joint_snapshot_umap{};
            UnorderedMap<ui32, RevoluteJoint2dSnapshot> revolute_joint_snapshot_umap{};
            DynamicArray<Collision2dEvent> collision_events{};
            entt::connection rb2d_construct{};
            entt::connection rb2d_destroy{};
            entt::connection box_construct{};
            entt::connection box_destroy{};
            entt::connection circle_construct{};
            entt::connection circle_destroy{};
            entt::connection distance_joint_construct{};
            entt::connection distance_joint_destroy{};
            entt::connection revolute_joint_construct{};
            entt::connection revolute_joint_destroy{};
        };

        RegistryState& getRegistryState(entt::registry& registry);
        void ensureState(Registry& reg);

        void onConstructRigidbody2d(entt::registry& registry, entt::entity entity);
        void onDestroyRigidbody2d(entt::registry& registry, entt::entity entity);
        void onConstructBoxCollider2d(entt::registry& registry, entt::entity entity);
        void onDestroyBoxCollider2d(entt::registry& registry, entt::entity entity);
        void onConstructCircleCollider2d(entt::registry& registry, entt::entity entity);
        void onDestroyCircleCollider2d(entt::registry& registry, entt::entity entity);
        void onConstructDistanceJoint2d(entt::registry& registry, entt::entity entity);
        void onDestroyDistanceJoint2d(entt::registry& registry, entt::entity entity);
        void onConstructRevoluteJoint2d(entt::registry& registry, entt::entity entity);
        void onDestroyRevoluteJoint2d(entt::registry& registry, entt::entity entity);

        b2BodyId createBody(RegistryState& state, entt::registry& registry, entt::entity entity);
        void destroyBody(RegistryState& state, entt::entity entity);
        b2BodyId findJointTargetBody(RegistryState& state, const UUID target_entity_uuid) const;
        void destroyBoxShape(RegistryState& state, entt::entity entity);
        void destroyCircleShape(RegistryState& state, entt::entity entity);
        void destroyShapes(RegistryState& state, entt::entity entity);
        void createBoxShape(RegistryState& state, entt::registry& registry, entt::entity entity);
        void createCircleShape(RegistryState& state, entt::registry& registry, entt::entity entity);
        void createDistanceJoint(RegistryState& state, entt::registry& registry, entt::entity entity);
        void createRevoluteJoint(RegistryState& state, entt::registry& registry, entt::entity entity);
        void destroyDistanceJoint(RegistryState& state, entt::entity entity);
        void destroyRevoluteJoint(RegistryState& state, entt::entity entity);
        void rebuildShapes(RegistryState& state, entt::registry& registry, entt::entity entity);
        void applyRequests(RegistryState& state, entt::registry& registry);
        void updateRuntimeParams(RegistryState& state, entt::registry& registry);
        void updateJoints(RegistryState& state, entt::registry& registry);
        void syncTransform(RegistryState& state, entt::registry& registry);
        void processCollisionEvents(RegistryState& state, entt::registry& registry);

        static b2BodyType RigidbodyTypeToBox2dType(Rigidbody2dComponent::BodyType type);
        static ui32 ShapeToEntity(const b2ShapeId shape_id);
        static bool IsSensorEntity(entt::registry& registry, ui32 entity_key);

        UnorderedMap<entt::registry*, RegistryState> m_registry_state_umap{};
    };

} // dodoe
