//
// Created by GreenMuffin on 2026/2/22.
//

#include "world_manager.h"

namespace dodoe {

	std::vector<Ref<World>> WorldManager::worlds_;

	const std::vector<Ref<World>>& WorldManager::worlds() {
		return worlds_;
	}

	int WorldManager::world_count() {
		return static_cast<int>(worlds_.size());
	}

	Ref<World> WorldManager::create_world(const std::string& name) {
		Ref<World> world = create_ref<World>(name);
		world->initialize();
		worlds_.push_back(world);
		return world;
	}

	Ref<World> WorldManager::get_world(const std::string& name) {
		for (const auto& world : worlds_) {
			if (world->name_ == name) {
				return world;
			}
		}
		DoError("Not found {}", name);
		return nullptr;
	}

	void WorldManager::destory_worlds() {
		for (const auto& world : worlds_) {
			world->shutdown();
		}
		worlds_.clear();
	}

} // dodoe