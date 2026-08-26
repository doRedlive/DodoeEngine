//
// Created by GreenMuffin on 2026/2/20.
//

#include "physics_system.h"

#include "runtime/function/world/scene.h"

namespace dodoe {

	void PhysicsSystem::step(const float dt) {
		m_accumulator += dt;
		m_last_step_count = 0;
		while (m_accumulator >= m_fixed_dt && m_last_step_count < m_max_sub_steps) {
			if (m_fixed_update_callback) {
				m_fixed_update_callback();
			}
			if (m_world_2d) {
				m_world_2d->step(m_fixed_dt);
			}
			if (m_world_3d) {
				m_world_3d->step(m_fixed_dt);
			}
			m_accumulator -= m_fixed_dt;
			++m_last_step_count;
		}
		if (m_last_step_count == m_max_sub_steps && m_accumulator >= m_fixed_dt) {
			m_accumulator = std::fmod(m_accumulator, m_fixed_dt);
		}
	}

	bool PhysicsSystem::initialize(const PhysicsSystemCreateInfo& create_info) {
		Physics2dWorldCreateInfo world_create_info;
		world_create_info.gravity = create_info.gravity;
		world_create_info.sub_step_count = create_info.sub_step_count;
		world_create_info.fixed_dt = create_info.fixed_dt;
		world_create_info.max_sub_steps = create_info.max_sub_steps;
		m_fixed_dt = create_info.fixed_dt;
		m_max_sub_steps = create_info.max_sub_steps;
		m_world_2d = Physics2dWorld::Create(world_create_info);
		m_world_3d = PhysicsWorld::Create({});
		return m_world_2d != nullptr && m_world_3d != nullptr;
	}

	void PhysicsSystem::shutdown() {
		Physics2dWorld::Destroy(m_world_2d);
		PhysicsWorld::Destroy(m_world_3d);
	}
}
