//
// Created by GreenMuffin on 2026/2/20.
//

#include "physics_system.h"

#include "runtime/function/world/scene.h"

namespace dodoe {

	Scope<PhysicsSystem> PhysicsSystem::create(const PhysicsSystemCreateInfo& create_info) {
		auto context = create_scope<PhysicsSystem>();
		context->initialize(create_info);
		return context;
	}

	void PhysicsSystem::destroy(Scope<PhysicsSystem>& physics_system) {
		if (!physics_system) return;
		physics_system->shutdown();
		physics_system.reset();
	}

	void PhysicsSystem::step(const float dt) {
		b2World_Step(world_id_, dt, sub_step_count_);
		b2World_Draw(world_id_, debugger_->native_debug_draw());
	}

	void PhysicsSystem::initialize(const PhysicsSystemCreateInfo& create_info) {
		world_id_ = b2WorldId();

		if (B2_IS_NON_NULL(world_id_)) {
			DO_ERROR("Can't create box2d world");
			return;
		}

		b2WorldDef world_def = b2DefaultWorldDef();
		world_def.gravity.x = 0.0f;
		world_def.gravity.y = create_info.gravity;
		world_id_ = b2CreateWorld(&world_def);

		debugger_ = PhysicsDebugger::create({});

		sub_step_count_ = create_info.sub_step_count;
	}

	void PhysicsSystem::shutdown() {
		PhysicsDebugger::destroy(debugger_);

		if (B2_IS_NON_NULL(world_id_)) {
			b2DestroyWorld(world_id_);
		}

		world_id_ = b2_nullWorldId;
	}
}