// do@Redlive

#include "physics_world.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Quat.h>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/MotionProperties.h>

#include <Jolt/RegisterTypes.h>

#include <Jolt/Renderer/DebugRenderer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>

namespace dodoe {

    namespace {

        Vector3f ToVector3(const JPH::Vec3& v) {
            return Vector3f(v.GetX(), v.GetY(), v.GetZ());
        }

        JPH::Vec3 ToVec3(const Vector3f& v) {
            return JPH::Vec3(v.x, v.y, v.z);
        }

        JPH::Quat ToJoltQuat(const Quaternion& q) {
            return JPH::Quat(q.x, q.y, q.z, q.w);
        }

        Quaternion ToGlmQuat(const JPH::Quat& q) {
            return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
        }

        UInt32 ToRgba32(const JPH::Color& color) {
            return (static_cast<UInt32>(color.r) << 24)
                 | (static_cast<UInt32>(color.g) << 16)
                 | (static_cast<UInt32>(color.b) << 8)
                 | static_cast<UInt32>(color.a);
        }

        constexpr ui64 kSensorBit = 1ull << 63;

        ui64 MakeShapeUserData(const bool is_sensor, const ui64 entity_key) {
            return (is_sensor ? kSensorBit : 0ull) | entity_key;
        }

        JPH::ObjectLayer MakeObjectLayer(const ui32 layer, const ui32 mask) {
            return static_cast<JPH::ObjectLayer>(((mask & 0xFFu) << 8) | (layer & 0xFFu));
        }

        JPH::EAllowedDOFs MakeAllowedDofs(const bool lock_rotation) {
            if (lock_rotation) {
                return JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
            }
            return JPH::EAllowedDOFs::All;
        }

        JPH::EMotionType ToMotionType(const RigidBodyType type) {
            switch (type) {
                case RigidBodyType::Static: return JPH::EMotionType::Static;
                case RigidBodyType::Dynamic: return JPH::EMotionType::Dynamic;
                case RigidBodyType::Kinematic: return JPH::EMotionType::Kinematic;
                default: return JPH::EMotionType::Static;
            }
        }

        class BroadPhaseLayerImpl final : public JPH::BroadPhaseLayerInterface {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override { return JPH::BroadPhaseLayer(0); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "Default"; }
#endif
        };

        class ObjectVsBroadPhaseImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
        public:
            bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
        };

        class ObjectLayerPairImpl final : public JPH::ObjectLayerPairFilter {
        public:
            bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
                const ui32 layer1 = inObject1 & 0xFFu;
                const ui32 mask1 = (inObject1 >> 8) & 0xFFu;
                const ui32 layer2 = inObject2 & 0xFFu;
                const ui32 mask2 = (inObject2 >> 8) & 0xFFu;
                return (layer1 & mask2) != 0 && (layer2 & mask1) != 0;
            }
        };

        struct ContactData {
            std::mutex mutex;
            DynamicArray<ContactEvent> events;
            UnorderedMap<ui64, ui64> body_shape_user_data;
        };

        class ContactListenerImpl final : public JPH::ContactListener {
        public:
            explicit ContactListenerImpl(ContactData* data) : m_data(data) {}

            void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
            void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
            void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

        private:
            void PushContact(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, const ContactPhase phase);
            bool IsSensorContact(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold) const;

            ContactData* m_data{nullptr};
        };

        void ContactListenerImpl::PushContact(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                              const JPH::ContactManifold& inManifold, const ContactPhase phase) {
            ContactEvent event;
            event.body_a = inBody1.GetID().GetIndexAndSequenceNumber();
            event.body_b = inBody2.GetID().GetIndexAndSequenceNumber();
            if (inManifold.mRelativeContactPointsOn1.size() > 0) {
                event.point = ToVector3(inManifold.GetWorldSpaceContactPointOn1(0));
            }
            event.normal = ToVector3(inManifold.mWorldSpaceNormal);
            event.relative_speed = (inBody1.GetLinearVelocity() - inBody2.GetLinearVelocity()).Dot(inManifold.mWorldSpaceNormal);
            event.phase = phase;
            std::lock_guard<std::mutex> lock(m_data->mutex);
            m_data->events.push_back(event);
        }

        bool ContactListenerImpl::IsSensorContact(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                                  const JPH::ContactManifold& inManifold) const {
            const ui64 user_data_1 = inBody1.GetShape()->GetSubShapeUserData(inManifold.mSubShapeID1);
            const ui64 user_data_2 = inBody2.GetShape()->GetSubShapeUserData(inManifold.mSubShapeID2);
            return (user_data_1 & kSensorBit) != 0 || (user_data_2 & kSensorBit) != 0;
        }

        void ContactListenerImpl::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                                 const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
            if (IsSensorContact(inBody1, inBody2, inManifold)) {
                ioSettings.mIsSensor = true;
            }
            PushContact(inBody1, inBody2, inManifold, ContactPhase::Begin);
        }

        void ContactListenerImpl::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                                     const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
            if (IsSensorContact(inBody1, inBody2, inManifold)) {
                ioSettings.mIsSensor = true;
            }
            PushContact(inBody1, inBody2, inManifold, ContactPhase::Hit);
        }

        void ContactListenerImpl::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) {
            ContactEvent event;
            event.body_a = inSubShapePair.GetBody1ID().GetIndexAndSequenceNumber();
            event.body_b = inSubShapePair.GetBody2ID().GetIndexAndSequenceNumber();
            event.phase = ContactPhase::End;
            std::lock_guard<std::mutex> lock(m_data->mutex);
            m_data->events.push_back(event);
        }

        class EntityGroupFilter final : public JPH::GroupFilter {
        public:
            ui64 m_entity_key{0};
            bool CanCollide(const JPH::CollisionGroup& inGroup1, const JPH::CollisionGroup& inGroup2) const override {
                (void)inGroup1;
                const JPH::GroupFilter* filter2 = inGroup2.GetGroupFilter();
                if (filter2 == nullptr) {
                    return true;
                }
                const auto* other = static_cast<const EntityGroupFilter*>(filter2);
                return m_entity_key != other->m_entity_key;
            }
        };

        class JoltDebugRendererImpl final : public JPH::DebugRenderer {
        public:
            explicit JoltDebugRendererImpl(PhysicsDebugger* debugger) : m_debugger(debugger) { Initialize(); }

            void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override {
                if (m_debugger) {
                    m_debugger->drawLine(ToVector3(inFrom), ToVector3(inTo), ToRgba32(inColor));
                }
            }

            void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, JPH::DebugRenderer::ECastShadow inCastShadow) override {
                (void)inCastShadow;
                DrawLine(inV1, inV2, inColor);
                DrawLine(inV2, inV3, inColor);
                DrawLine(inV3, inV1, inColor);
            }

            void DrawText3D(JPH::RVec3Arg, const JPH::string_view&, JPH::ColorArg, float) override {}

            JPH::DebugRenderer::Batch CreateTriangleBatch(const JPH::DebugRenderer::Triangle*, int) override { return JPH::DebugRenderer::Batch(); }
            JPH::DebugRenderer::Batch CreateTriangleBatch(const JPH::DebugRenderer::Vertex*, int, const JPH::uint32*, int) override { return JPH::DebugRenderer::Batch(); }

            void DrawGeometry(JPH::RMat44Arg, const JPH::AABox&, float, JPH::ColorArg, const JPH::DebugRenderer::GeometryRef&, JPH::DebugRenderer::ECullMode, JPH::DebugRenderer::ECastShadow, JPH::DebugRenderer::EDrawMode) override {}

        private:
            PhysicsDebugger* m_debugger{nullptr};

        public:
            static void operator delete(void* ptr, std::size_t size) noexcept;
        };

        void JoltDebugRendererImpl::operator delete(void* ptr, const std::size_t size) noexcept {
            Memory::DeallocatePersistent(ptr, size, AllocTag::Object);
        }

        struct LogicalBody {
            RigidBodyType type{RigidBodyType::Static};
            bool enabled{true};
            JPH::Ref<JPH::GroupFilter> group_filter;
            DynamicArray<JPH::BodyID> body_ids{};
            Vector3f position{};
            Quaternion rotation{};
            float gravity_scale{1.0f};
            float linear_damping{0.0f};
            float angular_damping{0.0f};
            bool lock_rotation{false};
            bool is_bullet{false};
            float mass_override{-1.0f};
            ui64 user_data{0};
        };

        JPH::BodyID CreateBodyForShape(JPH::PhysicsSystem& system, LogicalBody& lb, const ShapeCreateInfo& info,
                                       const JPH::ShapeRefC& shape, const ui64 entity_key) {
            JPH::BodyCreationSettings settings(
                shape.GetPtr(), ToVec3(lb.position), ToJoltQuat(lb.rotation),
                ToMotionType(lb.type), MakeObjectLayer(info.layer, info.mask));
            settings.mUserData = entity_key;
            settings.mGravityFactor = lb.gravity_scale;
            settings.mLinearDamping = lb.linear_damping;
            settings.mAngularDamping = lb.angular_damping;
            settings.mFriction = info.friction;
            settings.mRestitution = info.restitution;
            settings.mMotionQuality = lb.is_bullet ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
            settings.mAllowDynamicOrKinematic = true;
            settings.mAllowSleeping = true;
            settings.mAllowedDOFs = MakeAllowedDofs(lb.lock_rotation);
            if (lb.mass_override > 0.0f) {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = lb.mass_override;
            }
            settings.mCollisionGroup = JPH::CollisionGroup(lb.group_filter.GetPtr(), 0, 0);
            JPH::Body* body = system.GetBodyInterface().CreateBody(settings);
            JPH::BodyID body_id = body != nullptr ? body->GetID() : JPH::BodyID();
            if (lb.enabled && body != nullptr) {
                system.GetBodyInterface().AddBody(body_id, JPH::EActivation::Activate);
            }
            return body_id;
        }

    } // namespace

    struct PhysicsWorld::Impl {
        JPH::TempAllocatorImpl allocator;
        JPH::JobSystemThreadPool job_system;
        JPH::PhysicsSystem system;
        BroadPhaseLayerImpl broad_phase;
        ObjectVsBroadPhaseImpl object_vs_broad_phase;
        ObjectLayerPairImpl object_layer_pair;
        ContactData contact_data;
        ContactListenerImpl contact_listener;
        bool debug_enabled{false};
        float fixed_dt{1.0f / 60.0f};
        int max_sub_steps{4};
        float accumulator{0.0f};
        int last_step_count{0};
        Scope<PhysicsDebugger> debugger{nullptr};
        OwnPtr<JoltDebugRendererImpl> debug_renderer{nullptr};
        UnorderedMap<ui64, LogicalBody> bodies;
        UnorderedMap<ui64, JPH::BodyID> shape_bodies;
        UnorderedMap<ui64, ui64> shape_to_body;
        ui64 next_body_handle{1};
        ui64 next_shape_handle{1};

        explicit Impl(const PhysicsWorldCreateInfo& info)
            : allocator(16 * 1024 * 1024),
              job_system(512, 0, info.worker_threads > 0 ? info.worker_threads : -1),
              contact_listener(&contact_data) {
            fixed_dt = info.fixed_dt;
            max_sub_steps = info.max_sub_steps;
        }

        static void operator delete(void* ptr, std::size_t size) noexcept;
    };

    void PhysicsWorld::Impl::operator delete(void* ptr, const std::size_t size) noexcept {
        Memory::DeallocatePersistent(ptr, size, AllocTag::Object);
    }

    PhysicsWorld::~PhysicsWorld() {
        shutdown();
    }

    bool PhysicsWorld::initialize(const PhysicsWorldCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::initialize", "startup");
        JPH::RegisterDefaultAllocator();
        m_impl = create_own_ptr<Impl>(create_info);
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        m_impl->system.Init(65536, 0, 65536, 10240, m_impl->broad_phase, m_impl->object_vs_broad_phase, m_impl->object_layer_pair);
        m_impl->system.SetGravity(JPH::Vec3(create_info.gravity.x, create_info.gravity.y, create_info.gravity.z));
        m_impl->system.SetContactListener(&m_impl->contact_listener);
        m_impl->debugger = PhysicsDebugger::Create({});
        m_impl->debug_renderer = create_own_ptr<JoltDebugRendererImpl>(m_impl->debugger.get());
        return true;
    }

    void PhysicsWorld::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::shutdown", "shutdown");
        if (!m_impl) {
            return;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (auto& [handle, body] : m_impl->bodies) {
            (void)handle;
            for (const JPH::BodyID id : body.body_ids) {
                if (bi.IsAdded(id)) {
                    bi.RemoveBody(id);
                }
                bi.DestroyBody(id);
            }
        }
        m_impl->bodies.clear();
        m_impl->shape_bodies.clear();
        m_impl->shape_to_body.clear();
        m_impl->debug_renderer.reset();
        PhysicsDebugger::Destroy(m_impl->debugger);
        m_impl.reset();
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    void PhysicsWorld::step(const float dt) {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::step", "physics");
        Impl& impl = *m_impl;
        impl.accumulator += dt;
        impl.last_step_count = 0;
        while (impl.accumulator >= impl.fixed_dt && impl.last_step_count < impl.max_sub_steps) {
            DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::fixedUpdate", "physics");
            impl.system.Update(impl.fixed_dt, 1, &impl.allocator, &impl.job_system);
            impl.accumulator -= impl.fixed_dt;
            ++impl.last_step_count;
        }
        if (impl.last_step_count == impl.max_sub_steps && impl.accumulator >= impl.fixed_dt) {
            impl.accumulator = std::fmod(impl.accumulator, impl.fixed_dt);
        }
        if (impl.debug_enabled) {
            JPH::BodyManager::DrawSettings settings;
            settings.mDrawShape = true;
            settings.mDrawShapeWireframe = false;
            settings.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor;
            impl.system.DrawBodies(settings, impl.debug_renderer.get());
            impl.debugger->flush();
        }
    }

    void PhysicsWorld::takeContactEvents(DynamicArray<ContactEvent>& out_events) {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::takeContactEvents", "physics");
        out_events.clear();
        std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
        if (!m_impl->contact_data.events.empty()) {
            out_events.swap(m_impl->contact_data.events);
        }
    }

    ui64 PhysicsWorld::createBody(const BodyCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::createBody", "physics");
        LogicalBody lb;
        lb.type = info.type;
        lb.enabled = info.enabled;
        lb.position = info.position;
        lb.rotation = info.rotation;
        lb.gravity_scale = info.gravity_scale;
        lb.linear_damping = info.linear_damping;
        lb.angular_damping = info.angular_damping;
        lb.lock_rotation = info.lock_rotation;
        lb.is_bullet = info.is_bullet;
        lb.mass_override = info.mass_override;
        lb.user_data = info.user_data;
        auto* filter = new EntityGroupFilter();
        filter->m_entity_key = info.user_data;
        lb.group_filter = filter;
        const ui64 handle = m_impl->next_body_handle++;
        m_impl->bodies.emplace(handle, std::move(lb));
        return handle;
    }

    void PhysicsWorld::destroyBody(const ui64 body) {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::destroyBody", "physics");
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : it->second.body_ids) {
            if (bi.IsAdded(id)) {
                bi.RemoveBody(id);
            }
            bi.DestroyBody(id);
            std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
            m_impl->contact_data.body_shape_user_data.erase(id.GetIndexAndSequenceNumber());
        }
        for (auto sit = m_impl->shape_bodies.begin(); sit != m_impl->shape_bodies.end();) {
            const ui64 shape = sit->first;
            if (m_impl->shape_to_body[shape] == body) {
                m_impl->shape_to_body.erase(shape);
                sit = m_impl->shape_bodies.erase(sit);
            } else {
                ++sit;
            }
        }
        m_impl->bodies.erase(it);
    }

    void PhysicsWorld::setBodyType(const ui64 body, const RigidBodyType type) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        if (lb.type == type) {
            return;
        }
        lb.type = type;
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            bi.SetMotionType(id, ToMotionType(type), JPH::EActivation::Activate);
        }
    }

    void PhysicsWorld::setBodyEnabled(const ui64 body, const bool enabled) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        if (lb.enabled == enabled) {
            return;
        }
        lb.enabled = enabled;
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            if (enabled) {
                bi.AddBody(id, JPH::EActivation::Activate);
            } else {
                bi.RemoveBody(id);
            }
        }
    }

    void PhysicsWorld::setGravityScale(const ui64 body, const float scale) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.gravity_scale = scale;
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            bi.SetGravityFactor(id, scale);
        }
    }

    void PhysicsWorld::setLinearDamping(const ui64 body, const float damping) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.linear_damping = damping;
        const JPH::BodyLockInterface& lock_if = m_impl->system.GetBodyLockInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            JPH::BodyLockWrite lock(lock_if, id);
            if (lock.Succeeded()) {
                JPH::MotionProperties* mp = lock.GetBody().GetMotionPropertiesUnchecked();
                if (mp) {
                    mp->SetLinearDamping(damping);
                }
            }
        }
    }

    void PhysicsWorld::setAngularDamping(const ui64 body, const float damping) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.angular_damping = damping;
        const JPH::BodyLockInterface& lock_if = m_impl->system.GetBodyLockInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            JPH::BodyLockWrite lock(lock_if, id);
            if (lock.Succeeded()) {
                JPH::MotionProperties* mp = lock.GetBody().GetMotionPropertiesUnchecked();
                if (mp) {
                    mp->SetAngularDamping(damping);
                }
            }
        }
    }

    void PhysicsWorld::setFixedRotation(const ui64 body, const bool fixed) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.lock_rotation = fixed;
        const JPH::EAllowedDOFs dofs = MakeAllowedDofs(fixed);
        const JPH::BodyLockInterface& lock_if = m_impl->system.GetBodyLockInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            JPH::BodyLockWrite lock(lock_if, id);
            if (lock.Succeeded()) {
                JPH::Body& jolt_body = lock.GetBody();
                JPH::MotionProperties* mp = jolt_body.GetMotionPropertiesUnchecked();
                if (mp) {
                    mp->SetMassProperties(dofs, jolt_body.GetShape()->GetMassProperties());
                }
            }
        }
    }

    void PhysicsWorld::setBullet(const ui64 body, const bool bullet) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.is_bullet = bullet;
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            bi.SetMotionQuality(id, bullet ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete);
        }
    }

    void PhysicsWorld::setMassOverride(const ui64 body, const float mass) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.mass_override = mass;
        const JPH::BodyLockInterface& lock_if = m_impl->system.GetBodyLockInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            JPH::BodyLockWrite lock(lock_if, id);
            if (lock.Succeeded()) {
                JPH::Body& jolt_body = lock.GetBody();
                JPH::MotionProperties* mp = jolt_body.GetMotionPropertiesUnchecked();
                if (mp) {
                    JPH::MassProperties mass_props = jolt_body.GetShape()->GetMassProperties();
                    if (mass > 0.0f) {
                        mass_props.ScaleToMass(mass);
                    }
                    mp->SetMassProperties(MakeAllowedDofs(lb.lock_rotation), mass_props);
                }
            }
        }
    }

    void PhysicsWorld::teleportBody(const ui64 body, const Vector3f& position, const Quaternion& rotation) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        LogicalBody& lb = it->second;
        lb.position = position;
        lb.rotation = rotation;
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : lb.body_ids) {
            bi.SetPositionAndRotation(id, ToVec3(position), ToJoltQuat(rotation), JPH::EActivation::Activate);
        }
    }

    Vector3f PhysicsWorld::getBodyPosition(const ui64 body) const {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return {};
        }
        const LogicalBody& lb = it->second;
        if (lb.body_ids.empty()) {
            return lb.position;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        if (bi.IsAdded(lb.body_ids[0])) {
            return ToVector3(bi.GetPosition(lb.body_ids[0]));
        }
        return lb.position;
    }

    Quaternion PhysicsWorld::getBodyRotation(const ui64 body) const {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return {};
        }
        const LogicalBody& lb = it->second;
        if (lb.body_ids.empty()) {
            return lb.rotation;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        if (bi.IsAdded(lb.body_ids[0])) {
            return ToGlmQuat(bi.GetRotation(lb.body_ids[0]));
        }
        return lb.rotation;
    }

    void PhysicsWorld::setBodyLinearVelocity(const ui64 body, const Vector3f& velocity) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : it->second.body_ids) {
            bi.SetLinearVelocity(id, ToVec3(velocity));
        }
    }

    void PhysicsWorld::applyBodyForce(const ui64 body, const Vector3f& force) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : it->second.body_ids) {
            bi.AddForce(id, ToVec3(force));
        }
    }

    void PhysicsWorld::applyBodyImpulse(const ui64 body, const Vector3f& impulse) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return;
        }
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        for (const JPH::BodyID id : it->second.body_ids) {
            bi.AddImpulse(id, ToVec3(impulse));
        }
    }

    ui64 PhysicsWorld::getBodyUserData(const ui64 body) const {
        std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
        const auto it = m_impl->contact_data.body_shape_user_data.find(body);
        if (it == m_impl->contact_data.body_shape_user_data.end()) {
            return 0;
        }
        return it->second & ~kSensorBit;
    }

    bool PhysicsWorld::isBodyActive(const ui64 body) const {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end() || it->second.body_ids.empty()) {
            return false;
        }
        return m_impl->system.GetBodyInterface().IsActive(it->second.body_ids[0]);
    }

    ui64 PhysicsWorld::createBoxShape(const ui64 body, const BoxShapeCreateInfo& info) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return 0;
        }
        if (info.half_extents.x <= 0.0f || info.half_extents.y <= 0.0f || info.half_extents.z <= 0.0f) {
            return 0;
        }
        LogicalBody& lb = it->second;

        JPH::BoxShapeSettings inner(JPH::Vec3(info.half_extents.x, info.half_extents.y, info.half_extents.z));
        inner.SetDensity(info.density);
        const ui64 entity_key = info.user_data != 0 ? info.user_data : lb.user_data;
        inner.mUserData = MakeShapeUserData(info.is_sensor, entity_key);

        JPH::RotatedTranslatedShapeSettings wrapped(ToVec3(info.offset), ToJoltQuat(Quaternion(info.rotation)), &inner);
        JPH::ShapeSettings::ShapeResult result = wrapped.Create();
        if (!result.IsValid()) {
            return 0;
        }

        JPH::BodyID body_id = CreateBodyForShape(m_impl->system, lb, info, result.Get(), entity_key);
        const ui64 shape_handle = m_impl->next_shape_handle++;
        lb.body_ids.push_back(body_id);
        m_impl->shape_bodies.emplace(shape_handle, body_id);
        m_impl->shape_to_body.emplace(shape_handle, body);
        {
            std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
            m_impl->contact_data.body_shape_user_data[body_id.GetIndexAndSequenceNumber()] = inner.mUserData;
        }
        return shape_handle;
    }

    ui64 PhysicsWorld::createSphereShape(const ui64 body, const SphereShapeCreateInfo& info) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return 0;
        }
        if (info.radius <= 0.0f) {
            return 0;
        }
        LogicalBody& lb = it->second;

        JPH::SphereShapeSettings inner(info.radius);
        inner.SetDensity(info.density);
        const ui64 entity_key = info.user_data != 0 ? info.user_data : lb.user_data;
        inner.mUserData = MakeShapeUserData(info.is_sensor, entity_key);

        JPH::RotatedTranslatedShapeSettings wrapped(ToVec3(info.offset), ToJoltQuat(Quaternion(info.rotation)), &inner);
        JPH::ShapeSettings::ShapeResult result = wrapped.Create();
        if (!result.IsValid()) {
            return 0;
        }

        JPH::BodyID body_id = CreateBodyForShape(m_impl->system, lb, info, result.Get(), entity_key);
        const ui64 shape_handle = m_impl->next_shape_handle++;
        lb.body_ids.push_back(body_id);
        m_impl->shape_bodies.emplace(shape_handle, body_id);
        m_impl->shape_to_body.emplace(shape_handle, body);
        {
            std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
            m_impl->contact_data.body_shape_user_data[body_id.GetIndexAndSequenceNumber()] = inner.mUserData;
        }
        return shape_handle;
    }

    ui64 PhysicsWorld::createCapsuleShape(const ui64 body, const CapsuleShapeCreateInfo& info) {
        auto it = m_impl->bodies.find(body);
        if (it == m_impl->bodies.end()) {
            return 0;
        }
        if (info.radius <= 0.0f || info.half_height < 0.0f) {
            return 0;
        }
        LogicalBody& lb = it->second;

        JPH::CapsuleShapeSettings inner(info.half_height, info.radius);
        inner.SetDensity(info.density);
        const ui64 entity_key = info.user_data != 0 ? info.user_data : lb.user_data;
        inner.mUserData = MakeShapeUserData(info.is_sensor, entity_key);

        JPH::RotatedTranslatedShapeSettings wrapped(ToVec3(info.offset), ToJoltQuat(Quaternion(info.rotation)), &inner);
        JPH::ShapeSettings::ShapeResult result = wrapped.Create();
        if (!result.IsValid()) {
            return 0;
        }

        JPH::BodyID body_id = CreateBodyForShape(m_impl->system, lb, info, result.Get(), entity_key);
        const ui64 shape_handle = m_impl->next_shape_handle++;
        lb.body_ids.push_back(body_id);
        m_impl->shape_bodies.emplace(shape_handle, body_id);
        m_impl->shape_to_body.emplace(shape_handle, body);
        {
            std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
            m_impl->contact_data.body_shape_user_data[body_id.GetIndexAndSequenceNumber()] = inner.mUserData;
        }
        return shape_handle;
    }

    void PhysicsWorld::destroyShape(const ui64 shape) {
        auto sit = m_impl->shape_bodies.find(shape);
        if (sit == m_impl->shape_bodies.end()) {
            return;
        }
        const JPH::BodyID body_id = sit->second;
        const ui64 body = m_impl->shape_to_body[shape];
        JPH::BodyInterface& bi = m_impl->system.GetBodyInterface();
        if (bi.IsAdded(body_id)) {
            bi.RemoveBody(body_id);
        }
        bi.DestroyBody(body_id);
        {
            std::lock_guard<std::mutex> lock(m_impl->contact_data.mutex);
            m_impl->contact_data.body_shape_user_data.erase(body_id.GetIndexAndSequenceNumber());
        }
        auto bit = m_impl->bodies.find(body);
        if (bit != m_impl->bodies.end()) {
            DynamicArray<JPH::BodyID>& ids = bit->second.body_ids;
            ids.erase(std::remove(ids.begin(), ids.end(), body_id), ids.end());
        }
        m_impl->shape_to_body.erase(shape);
        m_impl->shape_bodies.erase(sit);
    }

    void PhysicsWorld::raycast(const Vector3f& origin, const Vector3f& direction, const float max_distance,
                               const Query3dFilter& filter, DynamicArray<RaycastHit>& out_hits) const {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::raycast", "physics-query");
        out_hits.clear();
        const JPH::ObjectLayer object_layer = MakeObjectLayer(filter.layer, filter.mask);
        const JPH::NarrowPhaseQuery& query = m_impl->system.GetNarrowPhaseQuery();
        const JPH::RRayCast ray(ToVec3(origin), ToVec3(direction) * max_distance);
        JPH::RayCastSettings settings;
        settings.SetBackFaceMode(JPH::EBackFaceMode::IgnoreBackFaces);
        JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
        query.CastRay(
            ray, settings, collector,
            JPH::DefaultBroadPhaseLayerFilter(m_impl->object_vs_broad_phase, object_layer),
            JPH::DefaultObjectLayerFilter(m_impl->object_layer_pair, object_layer));
        collector.Sort();
        const JPH::BodyLockInterface& lock_if = m_impl->system.GetBodyLockInterface();
        for (const JPH::RayCastResult& result : collector.mHits) {
            RaycastHit hit;
            hit.body = result.mBodyID.GetIndexAndSequenceNumber();
            hit.point = ToVector3(ray.GetPointOnRay(result.mFraction));
            hit.normal = Vector3f(0.0f, 1.0f, 0.0f);
            JPH::BodyLockRead lock(lock_if, result.mBodyID);
            if (lock.Succeeded()) {
                hit.normal = ToVector3(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToVec3(hit.point)));
            }
            hit.fraction = result.mFraction;
            out_hits.push_back(hit);
        }
    }

    void PhysicsWorld::overlapBox(const Vector3f& center, const Vector3f& half_extents, const Quaternion& rotation,
                                  const Query3dFilter& filter, DynamicArray<OverlapHit>& out_hits) const {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::overlapBox", "physics-query");
        out_hits.clear();
        JPH::BoxShapeSettings shape_settings(JPH::Vec3(half_extents.x, half_extents.y, half_extents.z));
        JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
        if (!shape_result.IsValid()) {
            return;
        }
        JPH::CollideShapeSettings collide_settings;
        collide_settings.mMaxSeparationDistance = 0.0f;
        collide_settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const JPH::ObjectLayer object_layer = MakeObjectLayer(filter.layer, filter.mask);
        const JPH::NarrowPhaseQuery& query = m_impl->system.GetNarrowPhaseQuery();
        query.CollideShape(
            shape_result.Get().GetPtr(), JPH::Vec3::sOne(),
            JPH::RMat44::sRotationTranslation(ToJoltQuat(rotation), ToVec3(center)),
            collide_settings, ToVec3(center), collector,
            JPH::DefaultBroadPhaseLayerFilter(m_impl->object_vs_broad_phase, object_layer),
            JPH::DefaultObjectLayerFilter(m_impl->object_layer_pair, object_layer));
        collector.Sort();
        for (const JPH::CollideShapeResult& result : collector.mHits) {
            out_hits.push_back({ result.mBodyID2.GetIndexAndSequenceNumber() });
        }
    }

    void PhysicsWorld::overlapSphere(const Vector3f& center, const float radius,
                                     const Query3dFilter& filter, DynamicArray<OverlapHit>& out_hits) const {
        DO_PROFILE_SCOPE_CATEGORY("PhysicsWorld::overlapSphere", "physics-query");
        out_hits.clear();
        JPH::SphereShapeSettings shape_settings(radius);
        JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
        if (!shape_result.IsValid()) {
            return;
        }
        JPH::CollideShapeSettings collide_settings;
        collide_settings.mMaxSeparationDistance = 0.0f;
        collide_settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const JPH::ObjectLayer object_layer = MakeObjectLayer(filter.layer, filter.mask);
        const JPH::NarrowPhaseQuery& query = m_impl->system.GetNarrowPhaseQuery();
        query.CollideShape(
            shape_result.Get().GetPtr(), JPH::Vec3::sOne(),
            JPH::RMat44::sRotationTranslation(JPH::Quat::sIdentity(), ToVec3(center)),
            collide_settings, ToVec3(center), collector,
            JPH::DefaultBroadPhaseLayerFilter(m_impl->object_vs_broad_phase, object_layer),
            JPH::DefaultObjectLayerFilter(m_impl->object_layer_pair, object_layer));
        collector.Sort();
        for (const JPH::CollideShapeResult& result : collector.mHits) {
            out_hits.push_back({ result.mBodyID2.GetIndexAndSequenceNumber() });
        }
    }

    void PhysicsWorld::enableDebugDraw(const bool enabled) {
        m_impl->debug_enabled = enabled;
    }

    int PhysicsWorld::getLastStepCount() const {
        return m_impl->last_step_count;
    }

    int PhysicsWorld::getActiveBodyCount() const {
        return static_cast<int>(m_impl->system.GetNumActiveBodies(JPH::EBodyType::RigidBody));
    }

} // dodoe
