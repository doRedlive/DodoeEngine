//
// Created by GreenMuffin on 2026/2/22.
//

#include "world_manager.h"

namespace dodoe {

	WorldManager& WorldManager::self() {
		static WorldManager instance;
		return instance;
	}

	void WorldManager::initialize(WorldManagerInitInfo init_info) {
		DoAssert(init_info.renderer, "WorldManager::initialize: renderer is null.");
		destory_worlds();
		if (context_) {
			WorldContext::destroy(context_);
		}
		context_ = WorldContext::create({*init_info.renderer});
		create_world("default");
	}

	void WorldManager::shutdown() {
		destory_worlds();
		WorldContext::destroy(context_);
	}

	void WorldManager::runtime_update(const float delta_time) {
		active_world().runtime_update(delta_time);
	}

	void WorldManager::simulation_update(const float delta_time) {
		active_world().simulation_update(delta_time);
	}

	const std::vector<Scope<World>>& WorldManager::worlds() const {
		return worlds_;
	}

	int WorldManager::world_count() const {
		return static_cast<int>(worlds_.size());
	}

	World& WorldManager::create_world(const std::string& name) {
		DoAssert(context_, "WorldManager::create_world: context is not initialized.");
		for (auto it = worlds_.begin(); it != worlds_.end(); ++it) {
			if (!(*it) || (*it)->name_ != name) {
				continue;
			}

			if (it + 1 == worlds_.end()) {
				return *worlds_.back();
			}

			auto world = std::move(*it);
			worlds_.erase(it);
			worlds_.push_back(std::move(world));
			return *worlds_.back();
		}
		Scope<World> world = World::create({name, *context_});
		worlds_.push_back(std::move(world));
		return *worlds_.back();
	}

	World& WorldManager::get_world(const std::string& name) {
		for (const auto& world : worlds_) {
			if (world && world->name_ == name) {
				return *world;
			}
		}
		DoError("WorldManager::get_world: Not found {}", name);
		DoAssert(!worlds_.empty(), "WorldManager::get_world: worlds is empty.");
		return *worlds_.front();
	}

	void WorldManager::destory_worlds() {
		for (const auto& world : worlds_) {
			if (!world) {
				continue;
			}
			world->shutdown();
		}
		worlds_.clear();
	}

	World& WorldManager::active_world() {
		DoAssert(!worlds_.empty(), "WorldManager::active_world: no active world.");
		return *worlds_.back();
	}

} // dodoe
