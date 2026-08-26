// do@Redlive

#pragma once

#include "dopch.h"

#include "physics2d_world.h"
#include "physics_world.h"

namespace dodoe {

	struct PhysicsSystemCreateInfo {
		float gravity{-9.8f};
		int sub_step_count{4};
		float fixed_dt{1.0f / 60.0f};
		int max_sub_steps{4};
	};

	class PhysicsSystem : public Managed<PhysicsSystem, PhysicsSystemCreateInfo> {
        friend class Managed<PhysicsSystem, PhysicsSystemCreateInfo>;
	public:

		void step(float dt);

		void setFixedUpdateCallback(std::function<void()> callback) { m_fixed_update_callback = std::move(callback); }

		[[nodiscard]] float getFixedDt() const { return m_fixed_dt; }
		[[nodiscard]] int getLastStepCount() const { return m_last_step_count; }

		[[nodiscard]] Physics2dWorld* getWorld2d() { return m_world_2d.get(); }
		[[nodiscard]] const Physics2dWorld* getWorld2d() const { return m_world_2d.get(); }
		[[nodiscard]] PhysicsWorld* getWorld3d() { return m_world_3d.get(); }
		[[nodiscard]] const PhysicsWorld* getWorld3d() const { return m_world_3d.get(); }

	private:
		Scope<Physics2dWorld> m_world_2d{nullptr};
		Scope<PhysicsWorld> m_world_3d{nullptr};

		std::function<void()> m_fixed_update_callback{nullptr};
		float m_fixed_dt{1.0f / 60.0f};
		int m_max_sub_steps{4};
		float m_accumulator{0.0f};
		int m_last_step_count{0};

		bool initialize(const PhysicsSystemCreateInfo& create_info);
		void shutdown();
	};
} // dodoe
