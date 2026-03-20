//
// Created by GreenMuffin on 2026/3/12.
//

#include "registry.h"
#include "entity.h"

namespace dodoe {

	Registry::Registry(Scene* scene) : scene_context(scene) { }

	Entity Registry::create() {
		return registry_make_entity(scene_context, registry_.create());
	}

	void Registry::destroy(const Entity& entity) {
		registry_.destroy(registry_entity_handle(entity));
	}

	bool Registry::valid(const Entity& entity) const {
		return registry_.valid(registry_entity_handle(entity));
	}

} // dodoe