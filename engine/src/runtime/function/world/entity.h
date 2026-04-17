//
// Created by GreenMuffin on 2026/3/12.
//

#ifndef DODOE_ENTITY_H
#define DODOE_ENTITY_H

#include "dopch.h"

#include "scene.h"
#include "components.h"

#include "entt/entt.hpp"

namespace dodoe {
	class Entity;
	class Registry;
	entt::entity registry_entity_handle(const Entity& entity);
	Entity registry_make_entity(Scene* scene, entt::entity handle);

	class Entity {
		friend class Registry;
		friend class Scene;
		friend entt::entity registry_entity_handle(const Entity& entity);
		friend Entity registry_make_entity(Scene* scene, entt::entity handle);
	public:
		Entity() = default;
		Entity(entt::entity entity);
		Entity(Scene* scene, entt::entity entity);

		template<typename T, typename...Args>
		T& addComponent(Args&&... args) {
			DO_ASSERT(!hasComponent<T>(), "Entity already has the component!");
			T& component = scene_->reg_.emplace<T>(*this, std::forward<Args>(args)...);
			scene_->on_component_add_<T>(*this, component);
			return component;
		}

		template<typename T, typename...Args>
		T& addOrReplaceComponent(Args&&... args) {
			T& component = scene_->reg_.emplace_or_replace<T>(*this, std::forward<Args>(args)...);
			scene_->on_component_add_<T>(*this, component);
			return component;
		}

		template<typename T>
		T& getComponent() {
			DO_ASSERT(hasComponent<T>(), "Entity does not have the component!");
			return scene_->reg_.get<T>(*this);
		}

		template<typename T>
		bool hasComponent() {
			return scene_->reg_.all_of<T>(*this);
		}

		template<typename T>
		void removeComponent() {
			DO_ASSERT(hasComponent<T>(), "Entity does not have the component!");
			scene_->reg_.remove<T>(*this);
		}

		[[nodiscard]] static entt::entity nullEntity() { return entt::null; }
		[[nodiscard]] entt::entity handle() const { return handle_; }
		[[nodiscard]] bool valid() const { return handle_ != entt::null; }	
		[[nodiscard]] Uuid uuid() { return getComponent<IDComponent>().id; }	
		[[nodiscard]] const std::string& name() { return getComponent<IDComponent>().name; }
		
		explicit operator bool() const { return valid(); }
		bool operator==(const Entity&) const = default;
		operator ui32() const { return static_cast<ui32>(handle_); }

	private:
		[[nodiscard]] static Entity from_handle(const entt::entity handle) {
			Entity entity;
			entity.handle_ = handle;
			return entity;
		}

		entt::entity handle_{ entt::null };
		Scene* scene_{ nullptr };
	};

} // dodoe

#endif // !DODOE_ENTITY_H
