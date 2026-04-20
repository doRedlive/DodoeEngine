//
// Created by GreenMuffin on 2026/3/12.
//

#ifndef DODOE_ENTITY_H
#define DODOE_ENTITY_H

#include "dopch.h"

#include "registry.h"
#include "components.h"

#include "entt/entt.hpp"

namespace dodoe {
	class Entity;
	class Scene;
	class Registry;
	entt::entity GetEntityHandle_Help(const Entity& entity);
	Entity CreateEntityByScene_Help(Scene* scene, entt::entity handle);
	Registry& GetSceneRegitry_Help(Scene* scene);
	template<typename T>
	void OnComponentAdd_Help(Scene* scene, Entity entity, T& component);

	class Entity {
		friend class Registry;
		friend class Scene;
		friend entt::entity GetEntityHandle_Help(const Entity& entity);
		friend Entity CreateEntityByScene_Help(Scene* scene, entt::entity handle);
	public:
		Entity() = default;
		Entity(entt::entity entity);
		Entity(Scene* scene, entt::entity entity);

		template<typename T, typename...Args>
		T& addComponent(Args&&... args) {
			DO_ASSERT(!hasComponent<T>(), "Entity already has the component!");
			T& component = GetSceneRegitry_Help(scene_).emplace<T>(*this, std::forward<Args>(args)...);
			OnComponentAdd_Help(scene_, *this, component);
			return component;
		}

		template<typename T, typename...Args>
		T& addOrReplaceComponent(Args&&... args) {
			T& component = GetSceneRegitry_Help(scene_).emplace_or_replace<T>(*this, std::forward<Args>(args)...);
			OnComponentAdd_Help(scene_, *this, component);
			return component;
		}

		template<typename T>
		T& getComponent() {
			DO_ASSERT(hasComponent<T>(), "Entity does not have the component!");
			return GetSceneRegitry_Help(scene_).get<T>(*this);
		}

		template<typename T>
		bool hasComponent() {
			return GetSceneRegitry_Help(scene_).all_of<T>(*this);
		}

		template<typename T>
		void removeComponent() {
			DO_ASSERT(hasComponent<T>(), "Entity does not have the component!");
			GetSceneRegitry_Help(scene_).remove<T>(*this);
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
