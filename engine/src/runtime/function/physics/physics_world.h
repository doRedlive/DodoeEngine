// do@Redlive

#pragma once

#include "dopch.h"

#include "physics_debug.h"
#include "runtime/core/math/math.h"
#include "runtime/core/memory/own_ptr.h"

namespace dodoe {

    struct PhysicsWorldCreateInfo {
        Vector3f gravity{ 0.0f, -9.81f, 0.0f };
        float fixed_dt{ 1.0f / 60.0f };
        int max_sub_steps{ 4 };
        int worker_threads{ 0 };
    };

    enum class ContactPhase {
        Begin = 0,
        End = 1,
        Hit = 2
    };

    struct ContactEvent {
        ui64 body_a{ 0 };
        ui64 body_b{ 0 };
        Vector3f point{};
        Vector3f normal{};
        float relative_speed{ 0.0f };
        ContactPhase phase{ ContactPhase::Begin };
    };

    struct Query3dFilter {
        ui32 layer{ 1 };
        ui32 mask{ 0xFFFFFFFF };
    };

    struct RaycastHit {
        ui64 body{ 0 };
        Vector3f point{};
        Vector3f normal{};
        float fraction{ 0.0f };
    };

    struct OverlapHit {
        ui64 body{ 0 };
    };

    enum class RigidBodyType {
        Static = 0,
        Dynamic = 1,
        Kinematic = 2
    };

    struct BodyCreateInfo {
        RigidBodyType type{ RigidBodyType::Static };
        Vector3f position{};
        Quaternion rotation{};
        float gravity_scale{ 1.0f };
        float linear_damping{ 0.0f };
        float angular_damping{ 0.0f };
        bool lock_rotation{ false };
        bool is_bullet{ false };
        float mass_override{ -1.0f };
        bool enabled{ true };
        ui64 user_data{ 0 };
    };

    struct ShapeCreateInfo {
        Vector3f offset{};
        Vector3f rotation{};
        bool is_sensor{ false };
        ui32 layer{ 1 };
        ui32 mask{ 0xFFFFFFFF };
        float density{ 1.0f };
        float friction{ 0.5f };
        float restitution{ 0.0f };
        ui64 user_data{ 0 };
    };

    struct BoxShapeCreateInfo : ShapeCreateInfo {
        Vector3f half_extents{ 0.5f, 0.5f, 0.5f };
    };

    struct SphereShapeCreateInfo : ShapeCreateInfo {
        float radius{ 0.5f };
    };

    struct CapsuleShapeCreateInfo : ShapeCreateInfo {
        float radius{ 0.5f };
        float half_height{ 0.5f };
    };

    class PhysicsWorld : public Managed<PhysicsWorld, PhysicsWorldCreateInfo> {
        friend class Managed<PhysicsWorld, PhysicsWorldCreateInfo>;
    public:
        ~PhysicsWorld();

        void step(float dt);
        void takeContactEvents(DynamicArray<ContactEvent>& out_events);

        ui64 createBody(const BodyCreateInfo& info);
        void destroyBody(ui64 body);
        void setBodyType(ui64 body, RigidBodyType type);
        void setBodyEnabled(ui64 body, bool enabled);
        void setGravityScale(ui64 body, float scale);
        void setLinearDamping(ui64 body, float damping);
        void setAngularDamping(ui64 body, float damping);
        void setFixedRotation(ui64 body, bool fixed);
        void setBullet(ui64 body, bool bullet);
        void setMassOverride(ui64 body, float mass);
        void teleportBody(ui64 body, const Vector3f& position, const Quaternion& rotation);
        [[nodiscard]] Vector3f getBodyPosition(ui64 body) const;
        [[nodiscard]] Quaternion getBodyRotation(ui64 body) const;
        void setBodyLinearVelocity(ui64 body, const Vector3f& velocity);
        void applyBodyForce(ui64 body, const Vector3f& force);
        void applyBodyImpulse(ui64 body, const Vector3f& impulse);
        [[nodiscard]] ui64 getBodyUserData(ui64 body) const;
        [[nodiscard]] bool isBodyActive(ui64 body) const;

        ui64 createBoxShape(ui64 body, const BoxShapeCreateInfo& info);
        ui64 createSphereShape(ui64 body, const SphereShapeCreateInfo& info);
        ui64 createCapsuleShape(ui64 body, const CapsuleShapeCreateInfo& info);
        void destroyShape(ui64 shape);

        void raycast(const Vector3f& origin, const Vector3f& direction, float max_distance,
                     const Query3dFilter& filter, DynamicArray<RaycastHit>& out_hits) const;
        void overlapBox(const Vector3f& center, const Vector3f& half_extents, const Quaternion& rotation,
                        const Query3dFilter& filter, DynamicArray<OverlapHit>& out_hits) const;
        void overlapSphere(const Vector3f& center, float radius,
                           const Query3dFilter& filter, DynamicArray<OverlapHit>& out_hits) const;

        void enableDebugDraw(bool enabled);

        [[nodiscard]] int getLastStepCount() const;
        [[nodiscard]] int getActiveBodyCount() const;

    private:
        struct Impl;
        OwnPtr<Impl> m_impl{ nullptr };

        bool initialize(const PhysicsWorldCreateInfo& create_info);
        void shutdown();
    };

} // dodoe
