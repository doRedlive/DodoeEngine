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

    struct CollisionEvent {
        ui32 entity_a{ 0 };
        ui32 entity_b{ 0 };
        Vector3f point{};
        Vector3f normal{};
        float relative_speed{ 0.0f };
        bool is_sensor{ false };
        ContactPhase phase{ ContactPhase::Begin };
    };

    struct RaycastHit3d {
        ui32 entity{ 0 };
        Vector3f point{};
        Vector3f normal{};
        float fraction{ 0.0f };
    };

    struct PhysicsStats {
        ui32 step_count{ 0 };
        ui32 active_body_count{ 0 };
        float raycast_ms{ 0.0f };
    };

    struct Body3dSnapshot {
        RigidbodyComponent::BodyType type{};
        float gravity_scale{ 1.0f };
        float linear_damping{ 0.0f };
        float angular_damping{ 0.0f };
        float mass_override{ -1.0f };
        bool lock_rotation{ false };
        bool is_bullet{ false };
        bool enabled{ true };
        bool operator==(const Body3dSnapshot& other) const {
            return type == other.type
                && gravity_scale == other.gravity_scale
                && linear_damping == other.linear_damping
                && angular_damping == other.angular_damping
                && mass_override == other.mass_override
                && lock_rotation == other.lock_rotation
                && is_bullet == other.is_bullet
                && enabled == other.enabled;
        }
    };

    struct BoxShape3dSnapshot {
        Vector3f offset{};
        Vector3f rotation{};
        Vector3f size{};
        float density{};
        float friction{};
        float restitution{};
        bool is_sensor{};
        ui32 layer{};
        ui32 mask{};
        bool operator==(const BoxShape3dSnapshot& other) const {
            return offset == other.offset
                && rotation == other.rotation
                && size == other.size
                && density == other.density
                && friction == other.friction
                && restitution == other.restitution
                && is_sensor == other.is_sensor
                && layer == other.layer
                && mask == other.mask;
        }
    };

    struct SphereShape3dSnapshot {
        Vector3f offset{};
        Vector3f rotation{};
        float radius{};
        float density{};
        float friction{};
        float restitution{};
        bool is_sensor{};
        ui32 layer{};
        ui32 mask{};
        bool operator==(const SphereShape3dSnapshot& other) const {
            return offset == other.offset
                && rotation == other.rotation
                && radius == other.radius
                && density == other.density
                && friction == other.friction
                && restitution == other.restitution
                && is_sensor == other.is_sensor
                && layer == other.layer
                && mask == other.mask;
        }
    };

    struct CapsuleShape3dSnapshot {
        Vector3f offset{};
        Vector3f rotation{};
        float radius{};
        float half_height{};
        float density{};
        float friction{};
        float restitution{};
        bool is_sensor{};
        ui32 layer{};
        ui32 mask{};
        bool operator==(const CapsuleShape3dSnapshot& other) const {
            return offset == other.offset
                && rotation == other.rotation
                && radius == other.radius
                && half_height == other.half_height
                && density == other.density
                && friction == other.friction
                && restitution == other.restitution
                && is_sensor == other.is_sensor
                && layer == other.layer
                && mask == other.mask;
        }
    };

    struct Transform3dSnapshot {
        Vector3f position{};
        Vector3f rotation{};
        bool operator==(const Transform3dSnapshot& other) const {
            return position == other.position
                && rotation == other.rotation;
        }
    };

    class Physics3dSystem : public System {
    public:
        ~Physics3dSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void start(Registry& reg) override;
        void update(Registry& reg, float dt) override;
        void finalize(Registry& reg) override;

        [[nodiscard]] ui64 getBodyHandle(Registry& reg, const Entity& entity) const;
        void takeCollisionEvents(Registry& reg, DynamicArray<CollisionEvent>& out_events);
        void raycast(Registry& reg, const Vector3f& origin, const Vector3f& direction, float max_distance,
                     DynamicArray<RaycastHit3d>& out_hits) const;
        void overlapBox(Registry& reg, const Vector3f& center, const Vector3f& half_extents, const Quaternion& rotation,
                        DynamicArray<ui32>& out_entities) const;
        void overlapSphere(Registry& reg, const Vector3f& center, float radius,
                           DynamicArray<ui32>& out_entities) const;
        void setDebugDraw(bool enabled);
        void getStats(Registry& reg, PhysicsStats& out_stats) const;

    private:
        struct RegistryState {
            Scene* scene_context{ nullptr };
            UnorderedMap<ui32, ui64> body_umap{};
            UnorderedMap<ui32, ui64> box_shape_umap{};
            UnorderedMap<ui32, ui64> sphere_shape_umap{};
            UnorderedMap<ui32, ui64> capsule_shape_umap{};
            UnorderedMap<ui32, Vector3f> scale_umap{};
            UnorderedMap<ui32, Body3dSnapshot> body_snapshot_umap{};
            UnorderedMap<ui32, BoxShape3dSnapshot> box_snapshot_umap{};
            UnorderedMap<ui32, SphereShape3dSnapshot> sphere_snapshot_umap{};
            UnorderedMap<ui32, CapsuleShape3dSnapshot> capsule_snapshot_umap{};
            UnorderedMap<ui32, Transform3dSnapshot> last_transform_umap{};
            DynamicArray<CollisionEvent> collision_events{};
            entt::connection rb3d_construct{};
            entt::connection rb3d_destroy{};
            entt::connection box_construct{};
            entt::connection box_destroy{};
            entt::connection sphere_construct{};
            entt::connection sphere_destroy{};
            entt::connection capsule_construct{};
            entt::connection capsule_destroy{};
        };

        RegistryState& getRegistryState(entt::registry& registry);
        void ensureState(Registry& reg);

        void onConstructRigidbody(entt::registry& registry, entt::entity entity);
        void onDestroyRigidbody(entt::registry& registry, entt::entity entity);
        void onConstructBoxCollider(entt::registry& registry, entt::entity entity);
        void onDestroyBoxCollider(entt::registry& registry, entt::entity entity);
        void onConstructSphereCollider(entt::registry& registry, entt::entity entity);
        void onDestroySphereCollider(entt::registry& registry, entt::entity entity);
        void onConstructCapsuleCollider(entt::registry& registry, entt::entity entity);
        void onDestroyCapsuleCollider(entt::registry& registry, entt::entity entity);

        ui64 createBody(RegistryState& state, entt::registry& registry, entt::entity entity);
        void destroyBody(RegistryState& state, entt::entity entity);
        void destroyBoxShape(RegistryState& state, entt::entity entity);
        void destroySphereShape(RegistryState& state, entt::entity entity);
        void destroyCapsuleShape(RegistryState& state, entt::entity entity);
        void destroyShapes(RegistryState& state, entt::entity entity);
        void createBoxShape(RegistryState& state, entt::registry& registry, entt::entity entity);
        void createSphereShape(RegistryState& state, entt::registry& registry, entt::entity entity);
        void createCapsuleShape(RegistryState& state, entt::registry& registry, entt::entity entity);
        void rebuildShapes(RegistryState& state, entt::registry& registry, entt::entity entity);
        void applyRequests(RegistryState& state, entt::registry& registry);
        void updateRuntimeParams(RegistryState& state, entt::registry& registry);
        void syncTransform(RegistryState& state, entt::registry& registry);
        void syncBoneAttachments(RegistryState& state, entt::registry& registry);
        void processCollisionEvents(RegistryState& state, entt::registry& registry);

        static RigidBodyType RigidbodyTypeToJoltType(RigidbodyComponent::BodyType type);
        static ui32 BodyToEntity(const PhysicsWorld* world, ui64 body);
        static bool IsSensorEntity(entt::registry& registry, ui32 entity_key);

        UnorderedMap<entt::registry*, RegistryState> m_registry_state_umap{};
        bool m_debug_draw{ false };
        mutable float m_last_raycast_ms{ 0.0f };
    };

} // dodoe
