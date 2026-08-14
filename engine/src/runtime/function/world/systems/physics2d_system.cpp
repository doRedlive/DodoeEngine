// do@Redlive

#include "physics2d_system.h"

#include "runtime/function/world/scene.h"

namespace dodoe {

    Physics2dSystem::~Physics2dSystem() = default;

    SystemAccess Physics2dSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<Rigidbody2dComponent, BoxCollider2dComponent, CircleCollider2dComponent,
                             DistanceJoint2dComponent, RevoluteJoint2dComponent,
                             SetVelocity2dRequest, ApplyForce2dRequest, ApplyImpulse2dRequest>()
            .writesComponents<TransformComponent, SetVelocity2dRequest, ApplyForce2dRequest, ApplyImpulse2dRequest>()
            .hasStructuralChanges(true)
            .build();
    }

    void Physics2dSystem::start(Registry& reg) {
        ensureState(reg);
    }

    void Physics2dSystem::update(Registry& reg, const float dt) {
        ensureState(reg);

        auto& state = getRegistryState(reg.raw());
        updateRuntimeParams(state, reg.raw());
        updateJoints(state, reg.raw());
        applyRequests(state, reg.raw());

        syncTransform(state, reg.raw());
        processCollisionEvents(state, reg.raw());
    }

    void Physics2dSystem::finalize(Registry& reg) {
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end()) {
            return;
        }

        for (auto& [_, body_id] : it->second.body_umap) {
            if (B2_IS_NON_NULL(body_id)) {
                b2DestroyBody(body_id);
            }
        }
        for (auto& [_, joint_id] : it->second.distance_joint_umap) {
            if (B2_IS_NON_NULL(joint_id)) {
                b2DestroyJoint(joint_id);
            }
        }
        for (auto& [_, joint_id] : it->second.revolute_joint_umap) {
            if (B2_IS_NON_NULL(joint_id)) {
                b2DestroyJoint(joint_id);
            }
        }
        it->second.rb2d_construct.release();
        it->second.rb2d_destroy.release();
        it->second.box_construct.release();
        it->second.box_destroy.release();
        it->second.circle_construct.release();
        it->second.circle_destroy.release();
        it->second.distance_joint_construct.release();
        it->second.distance_joint_destroy.release();
        it->second.revolute_joint_construct.release();
        it->second.revolute_joint_destroy.release();
        m_registry_state_umap.erase(it);
    }

    b2BodyId Physics2dSystem::getBodyId(Registry& reg, const Entity& entity) const {
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end()) {
            return b2_nullBodyId;
        }

        const ui32 key = static_cast<ui32>(GetEntityHandle_Help(entity));
        if (auto body_it = it->second.body_umap.find(key); body_it != it->second.body_umap.end()) {
            return body_it->second;
        }
        return b2_nullBodyId;
    }

    void Physics2dSystem::takeCollisionEvents(Registry& reg, DynamicArray<Collision2dEvent>& out_events) {
        out_events.clear();
        const auto it = m_registry_state_umap.find(&reg.raw());
        if (it == m_registry_state_umap.end() || it->second.collision_events.empty()) {
            return;
        }
        out_events.swap(it->second.collision_events);
    }

    void Physics2dSystem::raycast(Registry& reg, const Vector2f& origin, const Vector2f& direction, const float max_distance,
                                  DynamicArray<RaycastHit2d>& out_hits) const {
        out_hits.clear();
        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return;
        }

        DynamicArray<Raycast2dHit> raw_hits{};
        world_2d->raycast(origin, direction, max_distance, {}, raw_hits);

        for (const auto& raw_hit : raw_hits) {
            const ui32 entity = ShapeToEntity(raw_hit.shape);
            if (entity == 0) {
                continue;
            }
            RaycastHit2d hit;
            hit.entity = entity;
            hit.point = raw_hit.point;
            hit.normal = raw_hit.normal;
            hit.fraction = raw_hit.fraction;
            out_hits.push_back(hit);
        }
    }

    void Physics2dSystem::overlapAABB(Registry& reg, const Vector2f& center, const Vector2f& half_size,
                                      DynamicArray<ui32>& out_entities) const {
        out_entities.clear();
        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return;
        }

        DynamicArray<b2ShapeId> raw_shapes{};
        world_2d->overlapAABB(center, half_size, {}, raw_shapes);

        for (const auto& shape : raw_shapes) {
            const ui32 entity = ShapeToEntity(shape);
            if (entity == 0) {
                continue;
            }
            out_entities.push_back(entity);
        }
    }

    Physics2dSystem::RegistryState& Physics2dSystem::getRegistryState(entt::registry& registry) {
        return m_registry_state_umap[&registry];
    }

    void Physics2dSystem::ensureState(Registry& reg) {
        auto& state = getRegistryState(reg.raw());
        if (state.rb2d_construct) {
            return;
        }

        auto& raw_reg = reg.raw();
        state.scene_context = reg.scene_context;
        state.rb2d_construct = raw_reg.on_construct<Rigidbody2dComponent>().connect<&Physics2dSystem::onConstructRigidbody2d>(*this);
        state.rb2d_destroy = raw_reg.on_destroy<Rigidbody2dComponent>().connect<&Physics2dSystem::onDestroyRigidbody2d>(*this);
        state.box_construct = raw_reg.on_construct<BoxCollider2dComponent>().connect<&Physics2dSystem::onConstructBoxCollider2d>(*this);
        state.box_destroy = raw_reg.on_destroy<BoxCollider2dComponent>().connect<&Physics2dSystem::onDestroyBoxCollider2d>(*this);
        state.circle_construct = raw_reg.on_construct<CircleCollider2dComponent>().connect<&Physics2dSystem::onConstructCircleCollider2d>(*this);
        state.circle_destroy = raw_reg.on_destroy<CircleCollider2dComponent>().connect<&Physics2dSystem::onDestroyCircleCollider2d>(*this);
        state.distance_joint_construct = raw_reg.on_construct<DistanceJoint2dComponent>().connect<&Physics2dSystem::onConstructDistanceJoint2d>(*this);
        state.distance_joint_destroy = raw_reg.on_destroy<DistanceJoint2dComponent>().connect<&Physics2dSystem::onDestroyDistanceJoint2d>(*this);
        state.revolute_joint_construct = raw_reg.on_construct<RevoluteJoint2dComponent>().connect<&Physics2dSystem::onConstructRevoluteJoint2d>(*this);
        state.revolute_joint_destroy = raw_reg.on_destroy<RevoluteJoint2dComponent>().connect<&Physics2dSystem::onDestroyRevoluteJoint2d>(*this);

        auto view = reg.view<Rigidbody2dComponent, TransformComponent>();
        for (auto entity : view) {
            const entt::entity handle = GetEntityHandle_Help(entity);
            const ui32 key = static_cast<ui32>(handle);
            if (state.body_umap.contains(key)) {
                continue;
            }
            if (!B2_IS_NON_NULL(createBody(state, raw_reg, handle))) {
                continue;
            }
            rebuildShapes(state, raw_reg, handle);
        }

        for (auto entity : raw_reg.view<DistanceJoint2dComponent>()) {
            createDistanceJoint(state, raw_reg, entity);
        }
        for (auto entity : raw_reg.view<RevoluteJoint2dComponent>()) {
            createRevoluteJoint(state, raw_reg, entity);
        }
    }

    void Physics2dSystem::onConstructRigidbody2d(entt::registry& registry, const entt::entity entity) {
        auto& state = getRegistryState(registry);
        const ui32 key = static_cast<ui32>(entity);
        if (state.body_umap.contains(key)) {
            return;
        }
        if (!registry.all_of<TransformComponent>(entity)) {
            return;
        }
        if (!B2_IS_NON_NULL(createBody(state, registry, entity))) {
            return;
        }
        rebuildShapes(state, registry, entity);
    }

    void Physics2dSystem::onDestroyRigidbody2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyBody(it->second, entity);
    }

    void Physics2dSystem::onConstructBoxCollider2d(entt::registry& registry, const entt::entity entity) {
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

    void Physics2dSystem::onDestroyBoxCollider2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyBoxShape(it->second, entity);
    }

    void Physics2dSystem::onConstructCircleCollider2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (!state.body_umap.contains(key) || state.circle_shape_umap.contains(key)) {
            return;
        }
        createCircleShape(state, registry, entity);
    }

    void Physics2dSystem::onDestroyCircleCollider2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyCircleShape(it->second, entity);
    }

    void Physics2dSystem::onConstructDistanceJoint2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (state.distance_joint_umap.contains(key)) {
            return;
        }
        createDistanceJoint(state, registry, entity);
    }

    void Physics2dSystem::onDestroyDistanceJoint2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyDistanceJoint(it->second, entity);
    }

    void Physics2dSystem::onConstructRevoluteJoint2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        const ui32 key = static_cast<ui32>(entity);
        auto& state = it->second;
        if (state.revolute_joint_umap.contains(key)) {
            return;
        }
        createRevoluteJoint(state, registry, entity);
    }

    void Physics2dSystem::onDestroyRevoluteJoint2d(entt::registry& registry, const entt::entity entity) {
        const auto it = m_registry_state_umap.find(&registry);
        if (it == m_registry_state_umap.end()) {
            return;
        }
        destroyRevoluteJoint(it->second, entity);
    }

    b2BodyId Physics2dSystem::createBody(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return b2_nullBodyId;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& rb2d = registry.get<Rigidbody2dComponent>(entity);

        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.type = RigidbodyTypeToBox2dType(rb2d.type);
        body_def.position = { transform.position.x, transform.position.y };
        body_def.rotation = b2MakeRot(transform.rotation.z);
        body_def.gravityScale = rb2d.gravity_scale;

        b2BodyId body_id = b2CreateBody(world_2d->getWorldId(), &body_def);
        if (!B2_IS_NON_NULL(body_id)) {
            DO_ERROR("Created b2body failed!");
            return b2_nullBodyId;
        }

        b2Body_SetFixedRotation(body_id, rb2d.fixed_rotation);

        const ui32 key = static_cast<ui32>(entity);
        state.body_umap[key] = body_id;
        state.scale_umap[key] = { transform.scale.x, transform.scale.y };
        state.body_snapshot_umap[key] = { rb2d.type, rb2d.gravity_scale, rb2d.fixed_rotation };
        return body_id;
    }

    void Physics2dSystem::destroyBody(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it != state.body_umap.end()) {
            for (auto it = state.distance_joint_umap.begin(); it != state.distance_joint_umap.end();) {
                const b2JointId joint_id = it->second;
                if (B2_IS_NON_NULL(joint_id) && b2Joint_IsValid(joint_id)
                    && (B2_ID_EQUALS(b2Joint_GetBodyA(joint_id), body_it->second)
                        || B2_ID_EQUALS(b2Joint_GetBodyB(joint_id), body_it->second))) {
                    b2DestroyJoint(joint_id);
                    it = state.distance_joint_umap.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = state.revolute_joint_umap.begin(); it != state.revolute_joint_umap.end();) {
                const b2JointId joint_id = it->second;
                if (B2_IS_NON_NULL(joint_id) && b2Joint_IsValid(joint_id)
                    && (B2_ID_EQUALS(b2Joint_GetBodyA(joint_id), body_it->second)
                        || B2_ID_EQUALS(b2Joint_GetBodyB(joint_id), body_it->second))) {
                    b2DestroyJoint(joint_id);
                    it = state.revolute_joint_umap.erase(it);
                } else {
                    ++it;
                }
            }
            b2DestroyBody(body_it->second);
            state.body_umap.erase(body_it);
        }
        state.distance_joint_snapshot_umap.erase(key);
        state.revolute_joint_snapshot_umap.erase(key);
        state.box_shape_umap.erase(key);
        state.circle_shape_umap.erase(key);
        state.scale_umap.erase(key);
        state.body_snapshot_umap.erase(key);
        state.box_snapshot_umap.erase(key);
        state.circle_snapshot_umap.erase(key);
    }

    void Physics2dSystem::destroyBoxShape(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.box_shape_umap.find(key); it != state.box_shape_umap.end()) {
            if (B2_IS_NON_NULL(it->second)) {
                b2DestroyShape(it->second);
            }
            state.box_shape_umap.erase(it);
        }
    }

    void Physics2dSystem::destroyCircleShape(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.circle_shape_umap.find(key); it != state.circle_shape_umap.end()) {
            if (B2_IS_NON_NULL(it->second)) {
                b2DestroyShape(it->second);
            }
            state.circle_shape_umap.erase(it);
        }
    }

    void Physics2dSystem::destroyShapes(RegistryState& state, const entt::entity entity) {
        destroyBoxShape(state, entity);
        destroyCircleShape(state, entity);
    }

    void Physics2dSystem::createBoxShape(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it == state.body_umap.end()) {
            return;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& bc2d = registry.get<BoxCollider2dComponent>(entity);

        const float half_width = std::abs(bc2d.size.x * transform.scale.x);
        const float half_height = std::abs(bc2d.size.y * transform.scale.y);
        if (half_width <= FLT_EPSILON || half_height <= FLT_EPSILON) {
            return;
        }

        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = bc2d.density;
        shape_def.isSensor = bc2d.is_sensor;
        shape_def.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(key));
        shape_def.filter.categoryBits = bc2d.layer;
        shape_def.filter.maskBits = bc2d.mask;
        if (bc2d.is_sensor) {
            shape_def.enableSensorEvents = true;
        } else {
            shape_def.enableContactEvents = true;
            shape_def.enableHitEvents = true;
        }
        shape_def.material.friction = bc2d.friction;
        shape_def.material.restitution = bc2d.restitution;
        shape_def.material.restitutionThreshold = bc2d.restitution_threshold;

        b2Polygon box = b2MakeOffsetBox(half_width, half_height, { bc2d.offset.x, bc2d.offset.y }, b2Rot_identity);
        b2ShapeId shape_id = b2CreatePolygonShape(body_it->second, &shape_def, &box);
        if (B2_IS_NON_NULL(shape_id)) {
            state.box_shape_umap[key] = shape_id;
        }
        state.box_snapshot_umap[key] = { bc2d.offset, bc2d.size, bc2d.density, bc2d.friction, bc2d.restitution, bc2d.restitution_threshold, bc2d.is_sensor, bc2d.layer, bc2d.mask };
    }

    void Physics2dSystem::createCircleShape(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_it = state.body_umap.find(key);
        if (body_it == state.body_umap.end()) {
            return;
        }

        auto& transform = registry.get<TransformComponent>(entity);
        auto& cc2d = registry.get<CircleCollider2dComponent>(entity);

        const float radius = std::abs(cc2d.radius * transform.scale.x);
        if (radius <= FLT_EPSILON) {
            return;
        }

        b2ShapeDef shape_def = b2DefaultShapeDef();
        shape_def.density = cc2d.density;
        shape_def.isSensor = cc2d.is_sensor;
        shape_def.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(key));
        shape_def.filter.categoryBits = cc2d.layer;
        shape_def.filter.maskBits = cc2d.mask;
        if (cc2d.is_sensor) {
            shape_def.enableSensorEvents = true;
        } else {
            shape_def.enableContactEvents = true;
            shape_def.enableHitEvents = true;
        }
        shape_def.material.friction = cc2d.friction;
        shape_def.material.restitution = cc2d.restitution;
        shape_def.material.restitutionThreshold = cc2d.restitution_threshold;

        const b2Circle circle{ { cc2d.offset.x, cc2d.offset.y }, radius };
        b2ShapeId shape_id = b2CreateCircleShape(body_it->second, &shape_def, &circle);
        if (B2_IS_NON_NULL(shape_id)) {
            state.circle_shape_umap[key] = shape_id;
        }
        state.circle_snapshot_umap[key] = { cc2d.offset, cc2d.radius, cc2d.density, cc2d.friction, cc2d.restitution, cc2d.restitution_threshold, cc2d.is_sensor, cc2d.layer, cc2d.mask };
    }

    b2BodyId Physics2dSystem::findJointTargetBody(RegistryState& state, const UUID target_entity_uuid) const {
        if (!state.scene_context || !target_entity_uuid.isValid()) {
            return b2_nullBodyId;
        }
        const Entity target_entity = state.scene_context->tryGetEntityByUUID(target_entity_uuid);
        if (!target_entity.valid()) {
            return b2_nullBodyId;
        }
        const ui32 target_key = static_cast<ui32>(GetEntityHandle_Help(target_entity));
        if (auto body_it = state.body_umap.find(target_key); body_it != state.body_umap.end()) {
            return body_it->second;
        }
        return b2_nullBodyId;
    }

    void Physics2dSystem::createDistanceJoint(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_a_it = state.body_umap.find(key);
        if (body_a_it == state.body_umap.end()) {
            return;
        }

        auto& joint = registry.get<DistanceJoint2dComponent>(entity);
        const b2BodyId body_b = findJointTargetBody(state, joint.target_entity);
        if (!B2_IS_NON_NULL(body_b)) {
            return;
        }

        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return;
        }

        b2DistanceJointDef joint_def = b2DefaultDistanceJointDef();
        joint_def.bodyIdA = body_a_it->second;
        joint_def.bodyIdB = body_b;
        joint_def.localAnchorA = { joint.local_anchor_a.x, joint.local_anchor_a.y };
        joint_def.localAnchorB = { joint.local_anchor_b.x, joint.local_anchor_b.y };
        joint_def.length = joint.length;
        joint_def.enableSpring = joint.frequency > 0.0f;
        joint_def.hertz = joint.frequency;
        joint_def.dampingRatio = joint.damping_ratio;

        const b2JointId joint_id = b2CreateDistanceJoint(world_2d->getWorldId(), &joint_def);
        if (B2_IS_NON_NULL(joint_id)) {
            state.distance_joint_umap[key] = joint_id;
        }
        state.distance_joint_snapshot_umap[key] = {
            joint.target_entity, joint.local_anchor_a, joint.local_anchor_b,
            joint.length, joint.frequency, joint.damping_ratio
        };
    }

    void Physics2dSystem::createRevoluteJoint(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        const auto body_a_it = state.body_umap.find(key);
        if (body_a_it == state.body_umap.end()) {
            return;
        }

        auto& joint = registry.get<RevoluteJoint2dComponent>(entity);
        const b2BodyId body_b = findJointTargetBody(state, joint.target_entity);
        if (!B2_IS_NON_NULL(body_b)) {
            return;
        }

        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return;
        }

        b2RevoluteJointDef joint_def = b2DefaultRevoluteJointDef();
        joint_def.bodyIdA = body_a_it->second;
        joint_def.bodyIdB = body_b;
        joint_def.localAnchorA = { joint.local_anchor_a.x, joint.local_anchor_a.y };
        joint_def.localAnchorB = { joint.local_anchor_b.x, joint.local_anchor_b.y };
        joint_def.enableLimit = joint.enable_limit;
        joint_def.lowerAngle = joint.lower_angle;
        joint_def.upperAngle = joint.upper_angle;
        joint_def.enableMotor = joint.enable_motor;
        joint_def.motorSpeed = joint.motor_speed;
        joint_def.maxMotorTorque = joint.max_motor_torque;

        const b2JointId joint_id = b2CreateRevoluteJoint(world_2d->getWorldId(), &joint_def);
        if (B2_IS_NON_NULL(joint_id)) {
            state.revolute_joint_umap[key] = joint_id;
        }
        state.revolute_joint_snapshot_umap[key] = {
            joint.target_entity, joint.local_anchor_a, joint.local_anchor_b,
            joint.enable_limit, joint.lower_angle, joint.upper_angle,
            joint.enable_motor, joint.motor_speed, joint.max_motor_torque
        };
    }

    void Physics2dSystem::destroyDistanceJoint(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.distance_joint_umap.find(key); it != state.distance_joint_umap.end()) {
            if (B2_IS_NON_NULL(it->second) && b2Joint_IsValid(it->second)) {
                b2DestroyJoint(it->second);
            }
            state.distance_joint_umap.erase(it);
        }
        state.distance_joint_snapshot_umap.erase(key);
    }

    void Physics2dSystem::destroyRevoluteJoint(RegistryState& state, const entt::entity entity) {
        const ui32 key = static_cast<ui32>(entity);
        if (auto it = state.revolute_joint_umap.find(key); it != state.revolute_joint_umap.end()) {
            if (B2_IS_NON_NULL(it->second) && b2Joint_IsValid(it->second)) {
                b2DestroyJoint(it->second);
            }
            state.revolute_joint_umap.erase(it);
        }
        state.revolute_joint_snapshot_umap.erase(key);
    }

    void Physics2dSystem::rebuildShapes(RegistryState& state, entt::registry& registry, const entt::entity entity) {
        destroyShapes(state, entity);
        if (registry.all_of<BoxCollider2dComponent>(entity)) {
            createBoxShape(state, registry, entity);
        }
        if (registry.all_of<CircleCollider2dComponent>(entity)) {
            createCircleShape(state, registry, entity);
        }
    }

    void Physics2dSystem::updateRuntimeParams(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<Rigidbody2dComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }

            auto& rb2d = registry.get<Rigidbody2dComponent>(entity);
            const Body2dSnapshot snapshot{ rb2d.type, rb2d.gravity_scale, rb2d.fixed_rotation };
            const auto snap_it = state.body_snapshot_umap.find(key);
            if (snap_it != state.body_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            const b2BodyType body_type = RigidbodyTypeToBox2dType(rb2d.type);
            if (b2Body_GetType(body_it->second) != body_type) {
                b2Body_SetType(body_it->second, body_type);
            }
            b2Body_SetGravityScale(body_it->second, rb2d.gravity_scale);
            b2Body_SetFixedRotation(body_it->second, rb2d.fixed_rotation);
            state.body_snapshot_umap[key] = snapshot;
        }

        for (auto entity : registry.view<BoxCollider2dComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            if (!state.body_umap.contains(key)) {
                continue;
            }

            auto& bc2d = registry.get<BoxCollider2dComponent>(entity);
            const BoxShape2dSnapshot snapshot{
                bc2d.offset, bc2d.size, bc2d.density, bc2d.friction,
                bc2d.restitution, bc2d.restitution_threshold, bc2d.is_sensor, bc2d.layer, bc2d.mask
            };
            const auto snap_it = state.box_snapshot_umap.find(key);
            if (snap_it != state.box_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyBoxShape(state, entity);
            createBoxShape(state, registry, entity);
            state.box_snapshot_umap[key] = snapshot;
        }

        for (auto entity : registry.view<CircleCollider2dComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            if (!state.body_umap.contains(key)) {
                continue;
            }

            auto& cc2d = registry.get<CircleCollider2dComponent>(entity);
            const CircleShape2dSnapshot snapshot{
                cc2d.offset, cc2d.radius, cc2d.density, cc2d.friction,
                cc2d.restitution, cc2d.restitution_threshold, cc2d.is_sensor, cc2d.layer, cc2d.mask
            };
            const auto snap_it = state.circle_snapshot_umap.find(key);
            if (snap_it != state.circle_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyCircleShape(state, entity);
            createCircleShape(state, registry, entity);
            state.circle_snapshot_umap[key] = snapshot;
        }
    }

    void Physics2dSystem::updateJoints(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<DistanceJoint2dComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            auto& joint = registry.get<DistanceJoint2dComponent>(entity);
            const DistanceJoint2dSnapshot snapshot{
                joint.target_entity, joint.local_anchor_a, joint.local_anchor_b,
                joint.length, joint.frequency, joint.damping_ratio
            };
            const auto snap_it = state.distance_joint_snapshot_umap.find(key);
            if (snap_it != state.distance_joint_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyDistanceJoint(state, entity);
            createDistanceJoint(state, registry, entity);
        }

        for (auto entity : registry.view<RevoluteJoint2dComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            auto& joint = registry.get<RevoluteJoint2dComponent>(entity);
            const RevoluteJoint2dSnapshot snapshot{
                joint.target_entity, joint.local_anchor_a, joint.local_anchor_b,
                joint.enable_limit, joint.lower_angle, joint.upper_angle,
                joint.enable_motor, joint.motor_speed, joint.max_motor_torque
            };
            const auto snap_it = state.revolute_joint_snapshot_umap.find(key);
            if (snap_it != state.revolute_joint_snapshot_umap.end() && snap_it->second == snapshot) {
                continue;
            }

            destroyRevoluteJoint(state, entity);
            createRevoluteJoint(state, registry, entity);
        }
    }

    void Physics2dSystem::applyRequests(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<Rigidbody2dComponent, SetVelocity2dRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            const auto& request = registry.get<SetVelocity2dRequest>(entity);
            b2Body_SetLinearVelocity(body_it->second, { request.velocity.x, request.velocity.y });
        }
        registry.clear<SetVelocity2dRequest>();

        for (auto entity : registry.view<Rigidbody2dComponent, ApplyForce2dRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            const auto& request = registry.get<ApplyForce2dRequest>(entity);
            b2Body_ApplyForceToCenter(body_it->second, { request.force.x, request.force.y }, true);
        }
        registry.clear<ApplyForce2dRequest>();

        for (auto entity : registry.view<Rigidbody2dComponent, ApplyImpulse2dRequest>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }
            const auto& request = registry.get<ApplyImpulse2dRequest>(entity);
            b2Body_ApplyLinearImpulseToCenter(body_it->second, { request.impulse.x, request.impulse.y }, true);
        }
        registry.clear<ApplyImpulse2dRequest>();
    }

    void Physics2dSystem::syncTransform(RegistryState& state, entt::registry& registry) {
        for (auto entity : registry.view<Rigidbody2dComponent, TransformComponent>()) {
            const ui32 key = static_cast<ui32>(entity);
            const auto body_it = state.body_umap.find(key);
            if (body_it == state.body_umap.end()) {
                continue;
            }

            auto& transform = registry.get<TransformComponent>(entity);

            const auto scale_it = state.scale_umap.find(key);
            const Vector3f& scale = transform.scale;
            if (scale_it == state.scale_umap.end()
                || scale_it->second.x != scale.x || scale_it->second.y != scale.y) {
                state.scale_umap[key] = { scale.x, scale.y };
                rebuildShapes(state, registry, entity);
            }

            const b2Vec2 position = b2Body_GetPosition(body_it->second);
            const b2Rot rotation = b2Body_GetRotation(body_it->second);
            transform.position.x = position.x;
            transform.position.y = position.y;
            transform.rotation.z = b2Rot_GetAngle(rotation);
        }
    }

    void Physics2dSystem::processCollisionEvents(RegistryState& state, entt::registry& registry) {
        const auto* world_2d = GetPhysicsSystem()->getWorld2d();
        if (!world_2d) {
            return;
        }

        DynamicArray<Contact2dEvent> raw_events{};
        world_2d->takeContactEvents(raw_events);

        state.collision_events.clear();
        for (const auto& raw_event : raw_events) {
            const ui32 entity_a = ShapeToEntity(raw_event.shape_a);
            const ui32 entity_b = ShapeToEntity(raw_event.shape_b);
            if (entity_a == 0 || entity_b == 0) {
                continue;
            }

            Collision2dEvent collision_event;
            collision_event.entity_a = entity_a;
            collision_event.entity_b = entity_b;
            collision_event.point = { raw_event.point.x, raw_event.point.y };
            collision_event.normal = { raw_event.normal.x, raw_event.normal.y };
            collision_event.relative_speed = raw_event.relative_speed;
            collision_event.is_sensor = IsSensorEntity(registry, entity_a) || IsSensorEntity(registry, entity_b);
            collision_event.phase = raw_event.phase;
            state.collision_events.push_back(collision_event);
        }
    }

    ui32 Physics2dSystem::ShapeToEntity(const b2ShapeId shape_id) {
        if (!b2Shape_IsValid(shape_id)) {
            return 0;
        }
        return static_cast<ui32>(reinterpret_cast<uintptr_t>(b2Shape_GetUserData(shape_id)));
    }

    bool Physics2dSystem::IsSensorEntity(entt::registry& registry, const ui32 entity_key) {
        const entt::entity entity = static_cast<entt::entity>(entity_key);
        if (registry.all_of<BoxCollider2dComponent>(entity) && registry.get<BoxCollider2dComponent>(entity).is_sensor) {
            return true;
        }
        if (registry.all_of<CircleCollider2dComponent>(entity) && registry.get<CircleCollider2dComponent>(entity).is_sensor) {
            return true;
        }
        return false;
    }

    b2BodyType Physics2dSystem::RigidbodyTypeToBox2dType(const Rigidbody2dComponent::BodyType type) {
        switch (type) {
            case Rigidbody2dComponent::BodyType::Static: return b2_staticBody;
            case Rigidbody2dComponent::BodyType::Dynamic: return b2_dynamicBody;
            case Rigidbody2dComponent::BodyType::Kinematic: return b2_kinematicBody;
            default: return b2_staticBody;
        }
    }

} // dodoe
