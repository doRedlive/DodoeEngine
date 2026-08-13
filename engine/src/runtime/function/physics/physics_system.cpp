//
// Created by GreenMuffin on 2026/2/20.
//

#include "physics_system.h"

#include "runtime/function/world/scene.h"

namespace dodoe {

	void PhysicsSystem::step(const float dt) {
		m_world_2d->step(dt);
	}

	bool PhysicsSystem::initialize(const PhysicsSystemCreateInfo& create_info) {
		Physics2dWorldCreateInfo world_create_info;
		world_create_info.gravity = create_info.gravity;
		world_create_info.sub_step_count = create_info.sub_step_count;
		m_world_2d = Physics2dWorld::Create(world_create_info);
		return m_world_2d != nullptr;
	}

	void PhysicsSystem::shutdown() {
		Physics2dWorld::Destroy(m_world_2d);
	}
}
