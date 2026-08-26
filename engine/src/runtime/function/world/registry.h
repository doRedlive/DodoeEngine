//
// Created by GreenMuffin on 2026/3/12.
//

#ifndef DODOE_REGISTRY_H
#define DODOE_REGISTRY_H

#include "dopch.h"

#include "entt/entt.hpp"

namespace dodoe {
	class Scene;
	class Entity;

	Entity CreateEntityByScene_Help(Scene* scene, entt::entity handle);
	entt::entity GetEntityHandle_Help(const Entity& entity);

	class Registry {
	public:
		Scene* scene_context{ nullptr };

		explicit Registry(Scene* scene);

		template<typename... Excludes>
		struct Exclude final {};

		template<typename RawView>
		class View {
		public:
			explicit View(Scene* scene, RawView view)
				: scene_(scene), view_(std::move(view)) {}

			class Iterator {
			public:
				explicit Iterator(Scene* scene, typename RawView::iterator iter)
					: scene_(scene), iter_(iter) {}

				Iterator& operator++() {
					++iter_;
					return *this;
				}

				bool operator==(const Iterator& other) const {
					return iter_ == other.iter_;
				}

				auto operator*() const {
					return CreateEntityByScene_Help(scene_, *iter_);
				}

			private:
				Scene* scene_{ nullptr };
				typename RawView::iterator iter_;
			};

			[[nodiscard]]
			Iterator begin() {
				return Iterator(scene_, view_.begin());
			}

			[[nodiscard]]
			Iterator end() {
				return Iterator(scene_, view_.end());
			}

			[[nodiscard]]
			auto size_hint() const {
				return view_.size_hint();
			}

			[[nodiscard]]
			bool empty() const {
				return view_.begin() == view_.end();
			}

			[[nodiscard]]
			bool contains(const Entity& entity) const {
				return view_.contains(GetEntityHandle_Help(entity));
			}

			template<typename Func>
			void each(Func&& func) {
				view_.each([this, &func](const entt::entity entity, auto&... components) {
					std::invoke(std::forward<Func>(func), CreateEntityByScene_Help(scene_, entity), components...);
				});
			}

			template<typename... Components>
			void use() {
				view_.template use<Components...>();
			}

		private:
			Scene* scene_{ nullptr };
			RawView view_;
		};

		Entity create();

		void destroy(const Entity& entity);

		void destroy(const entt::entity entity) {
			registry_.destroy(entity);
		}

		[[nodiscard]]
		bool valid(const Entity& entity) const;

		[[nodiscard]]
		bool valid(const entt::entity entity) const {
			return registry_.valid(entity);
		}

		template<typename T>
		void ensurePoolExists() {
			(void)registry_.template view<T>();
		}

		void clear() {
			registry_.clear();
		}

		template<typename Component, typename Compare>
		void sort(Compare&& compare) {
			registry_.template sort<Component>(std::forward<Compare>(compare));
		}

		template<typename Component, typename... Args>
		decltype(auto) emplace(const Entity& entity, Args&&... args) {
			return registry_.template emplace<Component>(GetEntityHandle_Help(entity), std::forward<Args>(args)...);
		}

		template<typename Component, typename... Args>
		decltype(auto) emplace(const entt::entity entity, Args&&... args) {
			return registry_.template emplace<Component>(entity, std::forward<Args>(args)...);
		}

		template<typename Component, typename... Args>
		decltype(auto) get_or_emplace(const Entity& entity, Args&&... args) {
			return registry_.template get_or_emplace<Component>(GetEntityHandle_Help(entity), std::forward<Args>(args)...);
		}

		template<typename Component, typename... Args>
		decltype(auto) get_or_emplace(const entt::entity entity, Args&&... args) {
			return registry_.template get_or_emplace<Component>(entity, std::forward<Args>(args)...);
		}

		template<typename Component, typename... Args>
		decltype(auto) emplace_or_replace(const Entity& entity, Args&&... args) {
			return registry_.template emplace_or_replace<Component>(GetEntityHandle_Help(entity), std::forward<Args>(args)...);
		}

		template<typename Component, typename... Args>
		decltype(auto) emplace_or_replace(const entt::entity entity, Args&&... args) {
			return registry_.template emplace_or_replace<Component>(entity, std::forward<Args>(args)...);
		}

		template<typename Component>
		Component& get(const Entity& entity) {
			return registry_.template get<Component>(GetEntityHandle_Help(entity));
		}

		template<typename Component>
		Component& get(const entt::entity entity) {
			return registry_.template get<Component>(entity);
		}

		template<typename Component>
		const Component& get(const Entity& entity) const {
			return registry_.template get<Component>(GetEntityHandle_Help(entity));
		}

		template<typename Component>
		const Component& get(const entt::entity entity) const {
			return registry_.template get<Component>(entity);
		}

		template<typename... Components>
		bool all_of(const Entity& entity) const {
			return registry_.template all_of<Components...>(GetEntityHandle_Help(entity));
		}

		template<typename... Components>
		bool all_of(const entt::entity entity) const {
			return registry_.template all_of<Components...>(entity);
		}

		template<typename... Components>
		bool any_of(const Entity& entity) const {
			return registry_.template any_of<Components...>(GetEntityHandle_Help(entity));
		}

		template<typename... Components>
		bool any_of(const entt::entity entity) const {
			return registry_.template any_of<Components...>(entity);
		}

		template<typename... Components>
		bool has(const Entity& entity) const {
			return all_of<Components...>(entity);
		}

		template<typename... Components>
		bool has(const entt::entity entity) const {
			return all_of<Components...>(entity);
		}

		template<typename... Components>
		void remove(const Entity& entity) {
			registry_.template remove<Components...>(GetEntityHandle_Help(entity));
		}

		template<typename... Components>
		void remove(const entt::entity entity) {
			registry_.template remove<Components...>(entity);
		}

		template<typename Component>
		void erase(const Entity& entity) {
			registry_.template erase<Component>(GetEntityHandle_Help(entity));
		}

		template<typename Component>
		void erase(const entt::entity entity) {
			registry_.template erase<Component>(entity);
		}

		template<typename... Components>
		[[nodiscard]]
		auto view() {
			using RawView = decltype(registry_.template view<Components...>());
			return View<RawView>(scene_context, registry_.template view<Components...>());
		}

		template<typename... Components>
		[[nodiscard]]
		auto view() const {
			using RawView = decltype(registry_.template view<Components...>());
			return View<RawView>(scene_context, registry_.template view<Components...>());
		}

		template<typename... Components, typename... Excludes>
		[[nodiscard]]
		auto view(Exclude<Excludes...>) {
			using RawView = decltype(registry_.template view<Components...>(entt::exclude<Excludes...>));
			return View<RawView>(scene_context, registry_.template view<Components...>(entt::exclude<Excludes...>));
		}

		template<typename... Components, typename... Excludes>
		[[nodiscard]]
		auto view(Exclude<Excludes...>) const {
			using RawView = decltype(registry_.template view<Components...>(entt::exclude<Excludes...>));
			return View<RawView>(scene_context, registry_.template view<Components...>(entt::exclude<Excludes...>));
		}

		auto storage() {
			return registry_.storage();
		}

		auto storage() const {
			return registry_.storage();
		}

		[[nodiscard]]
		entt::registry& raw() {
			return registry_;
		}

		[[nodiscard]]
		const entt::registry& raw() const {
			return registry_;
		}

	private:
		entt::registry registry_{};
	};

} // dodoe

#endif // !DODOE_REGISTRY_H
