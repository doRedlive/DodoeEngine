//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef DODOE_WORLD_MANAGER_H
#define DODOE_WORLD_MANAGER_H

#include "dopch.h"

#include "world.h"

namespace dodoe {

	class WorldManager {
	public:
		[[nodiscard]]
		static const std::vector<Ref<World>>& worlds();
		[[nodiscard]]
		static int world_count();
		static Ref<World> create_world(const std::string& name);
		[[nodiscard]]
		static Ref<World> get_world(const std::string& name);
		static void destory_worlds();

	private:
		static std::vector<Ref<World>> worlds_;
	};

} // dodoe

#endif//DODOE_WORLD_MANAGER_H
