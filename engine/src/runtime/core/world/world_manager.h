//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef DODOE_WORLD_MANAGER_H
#define DODOE_WORLD_MANAGER_H

#include "dopch.h"

#include "world.h"

#include "runtime/function/render/renderer.h"

namespace dodoe {

	struct WorldManagerInitInfo {
		Renderer* renderer;
	};

	class WorldManager {
	public:
		static WorldManager& self();

		WorldManager(const WorldManager&) = delete;
		WorldManager& operator=(const WorldManager&) = delete;
		WorldManager(WorldManager&&) = delete;
		WorldManager& operator=(WorldManager&&) = delete;

		void initialize(WorldManagerInitInfo init_info);
		void shutdown();

		void runtime_start();
		void runtime_update(float delta_time);
		void runtime_finalize();
		void simulation_update(float delta_time);

		World& create_world(const std::string& name);
		void destory_worlds();

		[[nodiscard]] const std::vector<Scope<World>>& worlds() const;
		[[nodiscard]] int world_count() const;
		[[nodiscard]] World& get_world(const std::string& name);
		[[nodiscard]] World& active_world();

	private:
		std::vector<Scope<World>> worlds_;
		Scope<WorldContext> context_{nullptr};
		
		WorldManager() = default;
	};

} // dodoe

#endif//DODOE_WORLD_MANAGER_H
