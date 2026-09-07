// do@Redlive

#include "entity.h"

#include "scene.h"

#include "runtime/function/world/components/active_component.h"
#include "runtime/function/world/components/hierarchy_component.h"

namespace dodoe {

	// Helper functions are now inline in scene.h

	void Entity::setActive(bool active) {
		if (!hasComponent<ActiveComponent>()) {
			addComponent<ActiveComponent>(active);
			return;
		}

		auto& active_component = getComponent<ActiveComponent>();
		if (active_component.active_self == active) {
			return;
		}
		active_component.setActive(active);
	}

	bool Entity::activeSelf() const {
		if (!hasComponent<ActiveComponent>()) {
			return true;
		}
		return getComponent<ActiveComponent>().active_self;
	}

	bool Entity::activeInHierarchy() const {
		if (!activeSelf()) {
			return false;
		}

		if (!hasComponent<HierarchyComponent>()) {
			return true;
		}

		Entity current = getComponent<HierarchyComponent>().parent;
		while (current.valid()) {
			if (!current.activeSelf()) {
				return false;
			}
			if (!current.hasComponent<HierarchyComponent>()) {
				break;
			}
			current = current.getComponent<HierarchyComponent>().parent;
		}
		return true;
	}

	bool Entity::activeInHierarchy(entt::registry& registry, const entt::entity entity) {
		constexpr int kMaxHierarchyDepth = 1024;

		entt::entity current = entity;
		int depth = 0;
		while (registry.valid(current)) {
			if (const auto* active = registry.try_get<ActiveComponent>(current);
				active && !active->active_self) {
				return false;
			}

			const auto* hierarchy = registry.try_get<HierarchyComponent>(current);
			if (!hierarchy || !hierarchy->parent.valid()) {
				return true;
			}

			if (++depth > kMaxHierarchyDepth) {
				DO_ERROR("Hierarchy cycle detected while checking active state!");
				return true;
			}
			current = hierarchy->parent.handle();
		}
		return true;
	}

} // dodoe
