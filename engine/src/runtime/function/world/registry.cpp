// do@Redlive

#include "registry.h"
#include "entity.h"

namespace dodoe {

	Registry::Registry(Scene* scene) : scene_context(scene) { }

	Entity Registry::create() {
		return CreateEntityByScene_Help(scene_context, registry_.create());
	}

	void Registry::destroy(const Entity& entity) {
		registry_.destroy(GetEntityHandle_Help(entity));
	}

	bool Registry::valid(const Entity& entity) const {
		return registry_.valid(GetEntityHandle_Help(entity));
	}

} // dodoe