//
// Created by GreenMuffin on 2026/3/12.
//

#include "entity.h"

namespace dodoe {

	entt::entity registry_entity_handle(const Entity& entity) {
		return entity.handle_;
	}

	Entity registry_make_entity(Scene* scene, const entt::entity handle) {
		return Entity(scene, handle);
	}

	Entity::Entity(entt::entity entity) : handle_(entity) { }

	Entity::Entity(Scene* scene, entt::entity entity) : scene_(scene), handle_(entity) { }

} // dodoe