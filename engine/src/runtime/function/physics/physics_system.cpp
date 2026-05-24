//
// Created by GreenMuffin on 2026/2/20.
//

#include "physics_system.h"

#include "runtime/function/world/scene.h"

namespace dodoe {

	void PhysicsSystem::step(const float dt) {
		b2World_Step(world_id_, dt, sub_step_count_);
		b2World_Draw(world_id_, debugger_->native_debug_draw());
	}

	bool PhysicsSystem::initialize(const PhysicsSystemCreateInfo& create_info) {
		world_id_ = b2WorldId();

		if (B2_IS_NON_NULL(world_id_)) {
			DO_ERROR("Can't create box2d world");
			return false;
		}

		b2WorldDef world_def = b2DefaultWorldDef();
		world_def.gravity.x = 0.0f;
		world_def.gravity.y = create_info.gravity;
		world_id_ = b2CreateWorld(&world_def);

		debugger_ = PhysicsDebugger::Create({});

		sub_step_count_ = create_info.sub_step_count;
		return debugger_ != nullptr;
	}

	void PhysicsSystem::shutdown() {
		PhysicsDebugger::Destroy(debugger_);

		if (B2_IS_NON_NULL(world_id_)) {
			b2DestroyWorld(world_id_);
		}

		world_id_ = b2_nullWorldId;
	}
}
