// do@Redlive

#include "physics3d_system.h"

#include <chrono>

#include "runtime/function/world/scene.h"

namespace dodoe {

    Physics3dSystem::~Physics3dSystem() = default;

    SystemAccess Physics3dSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<RigidbodyComponent, BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent,
                             SetVelocityRequest, ApplyForceRequest, ApplyImpulseRequest, TeleportRequest,
                             AnimationPoseComponent, AnimationDriveModeComponent, BoneAttachmentComponent, MeshRendererComponent>()
            .writesComponents<TransformComponent, SetVelocityRequest, ApplyForceRequest, ApplyImpulseRequest, TeleportRequest>()
            .hasStructuralChanges(true)
            .build();
    }

    void Physics3dSystem::start(Registry& reg) {
        ensureState(reg);
    }

    void Physics3dSystem::update(Registry& reg, const float dt) {
        (void)dt;
        ensureState(reg);

        auto& state = getRegistryState(reg.raw());
        updateRuntimeParams(state, reg.raw());
        applyRequests(state, reg.raw());

        syncTransform(state, reg.raw());
        syncBoneAttachments(state, reg.raw());
        processCollisionEvents(state, reg.raw());
    }

    void Physics3dSystem::finalize(Registry& reg) {
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end()) {
            return;
        }

        auto* world = GetPhysicsSystem()->getWorld3d();
        for (auto& [_, body] : it->second.body_umap) {
            if (world) {
                world->destroyBody(body);
            }
        }
        it->second.rb3d_construct.release();
        it->second.rb3d_destroy.release();
        it->second.box_construct.release();
        it->second.box_destroy.release();
        it->second.sphere_construct.release();
        it->second.sphere_destroy.release();
        it->second.capsule_construct.release();
        it->second.capsule_destroy.release();
        m_registry_state_umap.erase(it);
    }

    ui64 Physics3dSystem::getBodyHandle(Registry& reg, const Entity& entity) const {
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end()) {
            return 0;
        }

        const ui32 key = static_cast<ui32>(GetEntityHandle_Help(entity));
        if (auto body_it = it->second.body_umap.find(key); body_it != it->second.body_umap.end()) {
            return body_it->second;
        }
        return 0;
    }

    void Physics3dSystem::takeCollisionEvents(Registry& reg, DynamicArray<CollisionEvent>& out_events) {
        out_events.clear();
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end() || it->second.collision_events.empty()) {
            return;
        }
        out_events.swap(it->second.collision_events);
    }

    void Physics3dSystem::raycast(Registry& reg, const Vector3f& origin, const Vector3f& direction, const float max_distance,
                                  DynamicArray<RaycastHit3d>& out_hits) const {
        out_hits.clear();
        const auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        DynamicArray<RaycastHit> raw_hits{};
        const auto ray_start = std::chrono::steady_clock::now();
        world->raycast(origin, direction, max_distance, {}, raw_hits);
        m_last_raycast_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - ray_start).count();

        for (const auto& raw_hit : raw_hits) {
            const ui32 entity = BodyToEntity(world, raw_hit.body);
            if (entity == 0) {
                continue;
            }
            RaycastHit3d hit;
            hit.entity = entity;
            hit.point = raw_hit.point;
            hit.normal = raw_hit.normal;
            hit.fraction = raw_hit.fraction;
            out_hits.push_back(hit);
        }
    }

    void Physics3dSystem::overlapBox(Registry& reg, const Vector3f& center, const Vector3f& half_extents, const Quaternion& rotation,
                                     DynamicArray<ui32>& out_entities) const {
        out_entities.clear();
        const auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        DynamicArray<OverlapHit> raw_hits{};
        world->overlapBox(center, half_extents, rotation, {}, raw_hits);

        for (const auto& raw_hit : raw_hits) {
            const ui32 entity = BodyToEntity(world, raw_hit.body);
            if (entity == 0) {
                continue;
            }
            out_entities.push_back(entity);
        }
    }

    void Physics3dSystem::setDebugDraw(const bool enabled) {
        m_debug_draw = enabled;
        if (auto* world = GetPhysicsSystem()->getWorld3d()) {
            world->enableDebugDraw(enabled);
        }
    }

    void Physics3dSystem::getStats(Registry& reg, PhysicsStats& out_stats) const {
        (void)reg;
        out_stats = {};
        const auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }
        out_stats.step_count = static_cast<ui32>(world->getLastStepCount());
        out_stats.active_body_count = static_cast<ui32>(world->getActiveBodyCount());
        out_stats.raycast_ms = m_last_raycast_ms;
    }

    void Physics3dSystem::overlapSphere(Registry& reg, const Vector3f& center, const float radius,
                                        DynamicArray<ui32>& out_entities) const {
        out_entities.clear();
        const auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        DynamicArray<OverlapHit> raw_hits{};
        world->overlapSphere(center, radius, {}, raw_hits);

        for (const auto& raw_hit : raw_hits) {
            const ui32 entity = BodyToEntity(world, raw_hit.body);
            if (entity == 0) {
                continue;
            }
            out_entities.push_back(entity);
        }
    }

    Physics3dSystem::RegistryState& Physics3dSystem::getRegistryState(entt::registry& registry) {
        return m_registry_state_umap[&registry];
    }

    void Physics3dSystem::ensureState(Registry& reg) {
        auto& state = getRegistryState(reg.raw());
        if (state.rb3d_construct) {
            return;
        }

        auto& raw_reg = reg.raw();
        state.scene_context = reg.scene_context;
        state.rb3d_construct = raw_reg.on_construct<RigidbodyComponent>().connect<&Physics3dSystem::onConstructRigidbody>(*this);
        state.rb3d_destroy = raw_reg.on_destroy<RigidbodyComponent>().connect<&Physics3dSystem::onDestroyRigidbody>(*this);
        state.box_construct = raw_reg.on_construct<BoxColliderComponent>().connect<&Physics3dSystem::onConstructBoxCollider>(*this);
        state.box_destroy = raw_reg.on_destroy<BoxColliderComponent>().connect<&Physics3dSystem::onDestroyBoxCollider>(*this);
        state.sphere_construct = raw_reg.on_construct<SphereColliderComponent>().connect<&Physics3dSystem::onConstructSphereCollider>(*this);
        state.sphere_destroy = raw_reg.on_destroy<SphereColliderComponent>().connect<&Physics3dSystem::onDestroySphereCollider>(*this);
        state.capsule_construct = raw_reg.on_construct<CapsuleColliderComponent>().connect<&Physics3dSystem::onConstructCapsuleCollider>(*this);
        state.capsule_destroy = raw_reg.on_destroy<CapsuleColliderComponent>().connect<&Physics3dSystem::onDestroyCapsuleCollider>(*this);

        auto view = reg.view<RigidbodyComponent, TransformComponent>();
        for (auto entity : view) {
            const entt::entity handle = GetEntityHandle_Help(entity);
            const ui32 key = static_cast<ui32>(handle);
            if (state.body_umap.contains(key)) {
                continue;
            }
            if (createBody(state, raw_reg, handle) == 0) {
                continue;
            }
            rebuildShapes(state, raw_reg, handle);
        }
    }

    void Physics3dSystem::onConstructRigidbody(entt::registry& registry, const entt::entity entity) {
        auto& state = getRegistryState(registry);
        const ui32 key = static_cast<ui32>(entity);
        if (state.body_umap.contains(key)) {
            return;
        }
        if (!registry.all_of<TransformComponent>(entity)) {
            return;
        }
        if (createBody(state, registry, entity) == 0) {
            return;
        }
        rebuildShapes(state, registry, entity);
    }

    void Physics3dSystem::onDestroyRigidbody(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyBody(it->second, entity);
    }

    void Physics3dSystem::onConstructBoxCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (!state.body_umap.contains(key) || state.box_shape_umap.contains(key)) {
            return;
        }
        createBoxShape(state, registry, entity);
    }

    void Physics3dSystem::onDestroyBoxCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyBoxShape(it->second, entity);
    }

    void Physics3dSystem::onConstructSphereCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (!state.body_umap.contains(key) || state.sphere_shape_umap.contains(key)) {
            return;
        }
        createSphereShape(state, registry, entity);
    }

    void Physics3dSystem::onDestroySphereCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroySphereShape(it->second, entity);
    }

    void Physics3dSystem::onConstructCapsuleCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (!state.body_umap.contains(key) || state.capsule_shape_umap.contains(key)) {
            return;
        }
        createCapsuleShape(state, registry, entity);
    }

    void Physics3dSystem::onDestroyCapsuleCollider(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyCapsuleShape(it->second, entity);
    }

    ui64 Physics3dSystem::createBody(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return 0;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& rb = registry.get<RigidbodyComponent>(entity);

        BodyCreateInfo info;
        info.type = RigidbodyTypeToJoltType(rb.type);
        info.position = transform.position;
        info.rotation = Quaternion(transform.rotation);
        info.gravity_scale = rb.gravity_scale;
        info.linear_damping = rb.linear_damping;
        info.angular_damping = rb.angular_damping;
        info.lock_rotation = rb.lock_rotation;
        info.is_bullet = rb.is_bullet;
        info.mass_override = rb.mass_override;
        info.enabled = rb.enabled;
        info.user_data = static_cast<ui32>(entity);

        const ui64 body = world->createBody(info);
        if (body == 0) {
            DO_ERROR("Created Jolt body failed!");
            return 0;
        }

        const ui32 key = static_cast<ui32>(entity);
        state.body_umap[key] = body;
        state.scale_umap[key] = transform.scale;
        state.last_transform_umap[key] = { transform.position, transform.rotation };
        state.body_snapshot_umap[key] = {
            rb.type, rb.gravity_scale, rb.linear_damping, rb.angular_damping,
            rb.mass_override, rb.lock_rotation, rb.is_bullet, rb.enabled
        };
        return body;
    }

    void Physics3dSystem::destroyBody(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it != state.body_umap.end()) {
            auto* world = GetPhysicsSystem()->getWorld3d();
            if (world) {
                world->destroyBody(body_it->second);
            }
            state.body_umap.erase(body_it);
        }
        state.box_shape_umap.erase(key);
        state.sphere_shape_umap.erase(key);
        state.capsule_shape_umap.erase(key);
        state.scale_umap.erase(key);
        state.last_transform_umap.erase(key);
        state.body_snapshot_umap.erase(key);
        state.box_snapshot_umap.erase(key);
        state.sphere_snapshot_umap.erase(key);
        state.capsule_snapshot_umap.erase(key);
    }

    void Physics3dSystem::destroyBoxShape(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.box_shape_umap.find(key); it != state.box_shape_umap.end()) {
            auto* world = GetPhysicsSystem()->getWorld3d();
            if (world) {
                world->destroyShape(it->second);
            }
            state.box_shape_umap.erase(it);
        }
    }

    void Physics3dSystem::destroySphereShape(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.sphere_shape_umap.find(key); it != state.sphere_shape_umap.end()) {
            auto* world = GetPhysicsSystem()->getWorld3d();
            if (world) {
                world->destroyShape(it->second);
            }
            state.sphere_shape_umap.erase(it);
        }
    }

    void Physics3dSystem::destroyCapsuleShape(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.capsule_shape_umap.find(key); it != state.capsule_shape_umap.end()) {
            auto* world = GetPhysicsSystem()->getWorld3d();
            if (world) {
                world->destroyShape(it->second);
            }
            state.capsule_shape_umap.erase(it);
        }
    }

    void Physics3dSystem::destroyShapes(RegistryState& state, const entt::entity entity) {
        destroyBoxShape(state, entity);
        destroySphereShape(state, entity);
        destroyCapsuleShape(state, entity);
    }

    void Physics3dSystem::createBoxShape(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it == state.body_umap.end()) {
            return;
        }

        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& bc = registry.get<BoxColliderComponent>(entity);

        BoxShapeCreateInfo info;
        info.offset = bc.offset;
        info.rotation = bc.rotation;
        info.half_extents = Vector3f(bc.size.x * transform.scale.x, bc.size.y * transform.scale.y, bc.size.z * transform.scale.z) * 0.5f;
        info.is_sensor = bc.is_sensor;
        info.layer = bc.layer;
        info.mask = bc.mask;
        info.density = bc.density;
        info.friction = bc.friction;
        info.restitution = bc.restitution;
        info.user_data = key;

        const ui64 shape = world->createBoxShape(body_it->second, info);
        if (shape != 0) {
            state.box_shape_umap[key] = shape;
        }
    }

    void Physics3dSystem::createSphereShape(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it == state.body_umap.end()) {
            return;
        }

        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& sc = registry.get<SphereColliderComponent>(entity);

        const float uniform_scale = Math::Max(Math::Abs(transform.scale.x), Math::Max(Math::Abs(transform.scale.y), Math::Abs(transform.scale.z)));

        SphereShapeCreateInfo info;
        info.offset = sc.offset;
        info.rotation = sc.rotation;
        info.radius = sc.radius * uniform_scale;
        info.is_sensor = sc.is_sensor;
        info.layer = sc.layer;
        info.mask = sc.mask;
        info.density = sc.density;
        info.friction = sc.friction;
        info.restitution = sc.restitution;
        info.user_data = key;

        const ui64 shape = world->createSphereShape(body_it->second, info);
        if (shape != 0) {
            state.sphere_shape_umap[key] = shape;
        }
    }

    void Physics3dSystem::createCapsuleShape(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it == state.body_umap.end()) {
            return;
        }

        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& cc = registry.get<CapsuleColliderComponent>(entity);

        const float uniform_scale = Math::Max(Math::Abs(transform.scale.y), Math::Max(Math::Abs(transform.scale.x), Math::Abs(transform.scale.z)));

        CapsuleShapeCreateInfo info;
        info.offset = cc.offset;
        info.rotation = cc.rotation;
        info.radius = cc.radius * uniform_scale;
        info.half_height = cc.half_height * uniform_scale;
        info.is_sensor = cc.is_sensor;
        info.layer = cc.layer;
        info.mask = cc.mask;
        info.density = cc.density;
        info.friction = cc.friction;
        info.restitution = cc.restitution;
        info.user_data = key;

        const ui64 shape = world->createCapsuleShape(body_it->second, info);
        if (shape != 0) {
            state.capsule_shape_umap[key] = shape;
        }
    }

    void Physics3dSystem::rebuildShapes(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        destroyShapes(state, entity);
        if (registry.all_of<BoxColliderComponent>(entity)) {
            createBoxShape(state, registry, entity);
        }
        if (registry.all_of<SphereColliderComponent>(entity)) {
            createSphereShape(state, registry, entity);
        }
        if (registry.all_of<CapsuleColliderComponent>(entity)) {
            createCapsuleShape(state, registry, entity);
        }
    }

    void Physics3dSystem::updateRuntimeParams(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<RigidbodyComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }

            auto& rb = registry.get<RigidbodyComponent>(entity);
            const Body3dSnapshot snapshot{
                rb.type, rb.gravity_scale, rb.linear_damping, rb.angular_damping,
                rb.mass_override, rb.lock_rotation, rb.is_bullet, rb.enabled
            };
            const auto snap_it = state.body_snapshot_umap.find(key);
            if (snap_it != state.body_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            auto* world = GetPhysicsSystem()->getWorld3d();
            if (!world) {
                continue;
            }
            world->setBodyType(body_it->second, RigidbodyTypeToJoltType(rb.type));
            world->setGravityScale(body_it->second, rb.gravity_scale);
            world->setLinearDamping(body_it->second, rb.linear_damping);
            world->setAngularDamping(body_it->second, rb.angular_damping);
            world->setFixedRotation(body_it->second, rb.lock_rotation);
            world->setBullet(body_it->second, rb.is_bullet);
            world->setMassOverride(body_it->second, rb.mass_override);
            world->setBodyEnabled(body_it->second, rb.enabled);
            state.body_snapshot_umap[key] = snapshot;
        }

        for (auto entity : registry.view<BoxColliderComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            if (!state.body_umap.contains(key)) {
                continue;
            }

            auto& bc = registry.get<BoxColliderComponent>(entity);
            const BoxShape3dSnapshot snapshot{
                bc.offset, bc.rotation, bc.size, bc.density, bc.friction,
                bc.restitution, bc.is_sensor, bc.layer, bc.mask
            };
            const auto snap_it = state.box_snapshot_umap.find(key);
            if (snap_it != state.box_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyBoxShape(state, entity);
            createBoxShape(state, registry, entity);
            state.box_snapshot_umap[key] = snapshot;
        }

        for (auto entity : registry.view<SphereColliderComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            if (!state.body_umap.contains(key)) {
                continue;
            }

            auto& sc = registry.get<SphereColliderComponent>(entity);
            const SphereShape3dSnapshot snapshot{
                sc.offset, sc.rotation, sc.radius, sc.density, sc.friction,
                sc.restitution, sc.is_sensor, sc.layer, sc.mask
            };
            const auto snap_it = state.sphere_snapshot_umap.find(key);
            if (snap_it != state.sphere_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroySphereShape(state, entity);
            createSphereShape(state, registry, entity);
            state.sphere_snapshot_umap[key] = snapshot;
        }

        for (auto entity : registry.view<CapsuleColliderComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            if (!state.body_umap.contains(key)) {
                continue;
            }

            auto& cc = registry.get<CapsuleColliderComponent>(entity);
            const CapsuleShape3dSnapshot snapshot{
                cc.offset, cc.rotation, cc.radius, cc.half_height, cc.density, cc.friction,
                cc.restitution, cc.is_sensor, cc.layer, cc.mask
            };
            const auto snap_it = state.capsule_snapshot_umap.find(key);
            if (snap_it != state.capsule_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyCapsuleShape(state, entity);
            createCapsuleShape(state, registry, entity);
            state.capsule_snapshot_umap[key] = snapshot;
        }
    }

    void Physics3dSystem::applyRequests(RegistryState& state, entt::registry& registry) {
        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        for (auto entity : registry.view<RigidbodyComponent, SetVelocityRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            world->setBodyLinearVelocity(body_it->second, registry.get<SetVelocityRequest>(entity).velocity);
        }
        registry.clear<SetVelocityRequest>();

        for (auto entity : registry.view<RigidbodyComponent, ApplyForceRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            world->applyBodyForce(body_it->second, registry.get<ApplyForceRequest>(entity).force);
        }
        registry.clear<ApplyForceRequest>();

        for (auto entity : registry.view<RigidbodyComponent, ApplyImpulseRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            world->applyBodyImpulse(body_it->second, registry.get<ApplyImpulseRequest>(entity).impulse);
        }
        registry.clear<ApplyImpulseRequest>();

        for (auto entity : registry.view<RigidbodyComponent, TeleportRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            const auto& request = registry.get<TeleportRequest>(entity);
            world->teleportBody(body_it->second, request.position, request.rotation);
        }
        registry.clear<TeleportRequest>();
    }

    void Physics3dSystem::syncTransform(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<RigidbodyComponent, TransformComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }

            auto& transform = registry.get<TransformComponent>(entity);
            auto& rb = registry.get<RigidbodyComponent>(entity);

            const auto scale_it = state.scale_umap.find(key);
            if (scale_it == state.scale_umap.end() || scale_it->second != transform.scale) {
                state.scale_umap[key] = transform.scale;
                rebuildShapes(state, registry, entity);
            }

            auto* world = GetPhysicsSystem()->getWorld3d();
            if (!world) {
                continue;
            }

            if (rb.type == RigidbodyComponent::BodyType::Dynamic) {
                transform.position = world->getBodyPosition(body_it->second);
                transform.rotation = Math::EulerAngles(world->getBodyRotation(body_it->second));
            } else {
                const auto last_it = state.last_transform_umap.find(key);
                if (last_it != state.last_transform_umap.end()
                    && (last_it->second.position != transform.position
                        || last_it->second.rotation != transform.rotation)) {
                    world->teleportBody(body_it->second, transform.position, Quaternion(transform.rotation));
                }
                state.last_transform_umap[key] = { transform.position, transform.rotation };
            }
        }
    }

    void Physics3dSystem::syncBoneAttachments(RegistryState& state, entt::registry& registry) {
        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        for (auto entity : registry.view<AnimationPoseComponent, BoneAttachmentComponent, RigidbodyComponent, MeshRendererComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }

            const auto& rb = registry.get<RigidbodyComponent>(entity);
            if (rb.type != RigidbodyComponent::BodyType::Kinematic) {
                continue;
            }

            if (registry.all_of<AnimationDriveModeComponent>(entity)) {
                const auto mode = registry.get<AnimationDriveModeComponent>(entity).mode;
                if (mode == AnimationDriveModeComponent::DriveMode::Ragdoll
                    || mode == AnimationDriveModeComponent::DriveMode::Partial) {
                    continue;
                }
            }

            const auto& pose = registry.get<AnimationPoseComponent>(entity);
            if (pose.world_matrices.empty()) {
                continue;
            }

            const auto& mesh = registry.get<MeshRendererComponent>(entity);
            if (!mesh.skeleton) {
                continue;
            }

            const auto& attachment = registry.get<BoneAttachmentComponent>(entity);
            const Int32 bone = mesh.skeleton->findNode(attachment.bone_name);
            if (bone < 0 || bone >= static_cast<Int32>(pose.world_matrices.size())) {
                continue;
            }

            const Matrix4f& bone_matrix = pose.world_matrices[static_cast<Size_t>(bone)];
            const Quaternion bone_rotation = glm::quat_cast(bone_matrix);
            const Vector3f bone_position = Vector3f(bone_matrix[3]);
            const Vector3f target_position = bone_position + bone_rotation * attachment.local_offset;
            const Quaternion target_rotation = attachment.follow_rotation ? bone_rotation : Quaternion(1.0f, 0.0f, 0.0f, 0.0f);

            world->teleportBody(body_it->second, target_position, target_rotation);
        }
    }

    void Physics3dSystem::processCollisionEvents(RegistryState& state, entt::registry& registry) {
        auto* world = GetPhysicsSystem()->getWorld3d();
        if (!world) {
            return;
        }

        DynamicArray<ContactEvent> raw_events{};
        world->takeContactEvents(raw_events);

        state.collision_events.clear();
        for (const auto& raw_event : raw_events) {
            const ui32 entity_a = BodyToEntity(world, raw_event.body_a);
            const ui32 entity_b = BodyToEntity(world, raw_event.body_b);
            if (entity_a == 0 || entity_b == 0) {
                continue;
            }

            CollisionEvent collision_event;
            collision_event.entity_a = entity_a;
            collision_event.entity_b = entity_b;
            collision_event.point = raw_event.point;
            collision_event.normal = raw_event.normal;
            collision_event.relative_speed = raw_event.relative_speed;
            collision_event.is_sensor = IsSensorEntity(registry, entity_a) || IsSensorEntity(registry, entity_b);
            collision_event.phase = raw_event.phase;
            state.collision_events.push_back(collision_event);
        }
    }

    ui32 Physics3dSystem::BodyToEntity(const PhysicsWorld* world, const ui64 body) {
        return static_cast<ui32>(world->getBodyUserData(body));
    }

    bool Physics3dSystem::IsSensorEntity(entt::registry& registry, const ui32 entity_key) {
        const entt::entity entity = static_cast<entt::entity>(entity_key);
        if (registry.all_of<BoxColliderComponent>(entity) && registry.get<BoxColliderComponent>(entity).is_sensor) {
            return true;
        }
        if (registry.all_of<SphereColliderComponent>(entity) && registry.get<SphereColliderComponent>(entity).is_sensor) {
            return true;
        }
        if (registry.all_of<CapsuleColliderComponent>(entity) && registry.get<CapsuleColliderComponent>(entity).is_sensor) {
            return true;
        }
        return false;
    }

    RigidBodyType Physics3dSystem::RigidbodyTypeToJoltType(const RigidbodyComponent::BodyType type) {
        switch (type) {
            case RigidbodyComponent::BodyType::Static: return RigidBodyType::Static;
            case RigidbodyComponent::BodyType::Dynamic: return RigidBodyType::Dynamic;
            case RigidbodyComponent::BodyType::Kinematic: return RigidBodyType::Kinematic;
            default: return RigidBodyType::Static;
        }
    }

} // dodoe
