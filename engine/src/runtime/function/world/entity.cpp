//
// Created by GreenMuffin on 2026/3/12.
//

#include "entity.h"

#include "scene.h"

namespace dodoe {

	entt::entity GetEntityHandle_Help(const Entity& entity) {
		return entity.handle_;
	}

	Entity CreateEntityByScene_Help(Scene* scene, const entt::entity handle) {
		return Entity(scene, handle);
	}

	Registry& GetSceneRegitry_Help(Scene* scene) {
		return scene->registry();
	}

	Entity::Entity(entt::entity entity) : handle_(entity) { }

	Entity::Entity(Scene* scene, entt::entity entity) : scene_(scene), handle_(entity) { }

} // dodoe
