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
		T& add_component(Args&&... args) {
			DoAssert(!has_component<T>(), "Entity already has the component!");
			T& component = scene_->reg_.emplace<T>(*this, std::forward<Args>(args)...);
			scene_->on_component_add_<T>(*this, component);
			return component;
		}

		template<typename T, typename...Args>
		T& add_or_replace_component(Args&&... args) {
			T& component = scene_->reg_.emplace_or_replace<T>(*this, std::forward<Args>(args)...);
			scene_->on_component_add_<T>(*this, component);
			return component;
		}

		template<typename T>
		T& get_component() {
			DoAssert(has_component<T>(), "Entity does not have the component!");
			return scene_->reg_.get<T>(*this);
		}

		template<typename T>
		bool has_component() {
			return scene_->reg_.all_of<T>(*this);
		}

		template<typename T>
		void remove_component() {
			DoAssert(has_component<T>(), "Entity does not have the component!");
			scene_->reg_.remove<T>(*this);
		}

		[[nodiscard]]
		bool valid() const {
			return handle_ != entt::null;
		}

		explicit operator bool() const {
			return valid();
		}

		Uuid uuid() { return get_component<IDComponent>().id; }

		const std::string& name() { return get_component<IDComponent>().name; }

		bool operator==(const Entity&) const = default;

	private:

		[[nodiscard]]
		static Entity from_handle(const entt::entity handle) {
			Entity entity;
			entity.handle_ = handle;
			return entity;
		}

		[[nodiscard]]
		entt::entity handle() const {
			return handle_;
		}

		entt::entity handle_{ entt::null };
		Scene* scene_{ nullptr };
	};

} // dodoe

#endif // !DODOE_ENTITY_H
