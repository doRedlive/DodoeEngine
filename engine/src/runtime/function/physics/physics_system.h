//
// Created by GreenMuffin on 2026/2/20.
//

#ifndef DODOE_PHYSICS2D_H
#define DODOE_PHYSICS2D_H

#include "dopch.h"

#include "physics2d_world.h"
#include "physics_world.h"

namespace dodoe {

	struct PhysicsSystemCreateInfo {
		float gravity{-9.8f};
		int sub_step_count{4};
	};

	class PhysicsSystem : public Managed<PhysicsSystem, PhysicsSystemCreateInfo> {
        friend class Managed<PhysicsSystem, PhysicsSystemCreateInfo>;
	public:

		void step(float dt);

		[[nodiscard]] Physics2dWorld* getWorld2d() { return m_world_2d.get(); }
		[[nodiscard]] const Physics2dWorld* getWorld2d() const { return m_world_2d.get(); }
		[[nodiscard]] PhysicsWorld* getWorld3d() { return m_world_3d.get(); }
		[[nodiscard]] const PhysicsWorld* getWorld3d() const { return m_world_3d.get(); }

	private:
		Scope<Physics2dWorld> m_world_2d{nullptr};
		Scope<PhysicsWorld> m_world_3d{nullptr};

		bool initialize(const PhysicsSystemCreateInfo& create_info);
		void shutdown();
	};
}

#endif//!DODOE_PHYSICS2D_H
