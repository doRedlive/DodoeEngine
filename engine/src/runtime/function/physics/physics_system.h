//
// Created by GreenMuffin on 2026/2/20.
//

#ifndef DODOE_PHYSICS2D_H
#define DODOE_PHYSICS2D_H

#include "dopch.h"

#include "physics_debug.h"

#include "box2d/box2d.h"
#include "box2d/types.h"

namespace dodoe {

	class Scene;
	class Physics2dSystem;

	struct PhysicsSystemCreateInfo {
		float gravity{-9.8f};
		int sub_step_count{4};
	};

	class PhysicsSystem {
		friend class Physics2dSystem;
	public:
		static Scope<PhysicsSystem> create(const PhysicsSystemCreateInfo& create_info);
		static void destroy(Scope<PhysicsSystem>& physics_system);

		void step(float dt);

	private:
		b2WorldId world_id_{};
		int sub_step_count_{0};

		Scope<PhysicsDebugger> debugger_{nullptr};

		void initialize(const PhysicsSystemCreateInfo& create_info);
		void shutdown();
		
	};
}

#endif//!DODOE_PHYSICS2D_H
