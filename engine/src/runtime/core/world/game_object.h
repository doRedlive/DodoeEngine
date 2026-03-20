//
// Created by GreenMuffin on 2025/11/15.
//

#ifndef DODOE_GAME_OBJECT_H
#define DODOE_GAME_OBJECT_H
#include "dopch.h"

#include "runtime/core/base.h"
#include "runtime/core/utils/uuid.h"

#include "entt/entt.hpp"

namespace cakery {
    class HierarchyPanel;
}

namespace dodoe {
    class Scene;

    class GameObject {
        friend class Scene;
        friend class cakery::HierarchyPanel;
    public:
        GameObject();
        GameObject(entt::entity entity, Scene* scene);
        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = default;
        GameObject& operator=(GameObject&&) = default;
        ~GameObject();

        [[nodiscard]]
        const Uuid& uuid() const;

        [[nodiscard]] 
        const std::string& get_name() const ;
        void set_name(const std::string& name);

        [[nodiscard]] 
        bool has_parent() const;
        [[nodiscard]] 
        GameObject* get_parent() const;

        GameObject* add_child(const std::string& name);
        GameObject* add_child();

        [[nodiscard]] 
        GameObject* get_child(const std::string& name) const;
        [[nodiscard]] 
        std::vector<GameObject*> get_children() const;

        void remove_child(const std::string& name);
        void remove_all_children();

        void destroy();

        template <typename T, typename... Args>
        T& add_component(Args&&... args);
        void add_component(const std::string& class_name);

        template <typename T>
        T& get_component();

        template <typename T>
        [[nodiscard]] bool has_component() const;
        [[nodiscard]] bool has_component(const std::string& name) const;

        template <typename T>
        void remove_component() const;
        void remove_component(const std::string& class_name) const;

        [[nodiscard]] std::vector<entt::type_info> get_all_components() const;
        [[nodiscard]] void* get_component_value_ptr(const std::string& class_name) const;

    private:
        entt::id_type index_ {};
        Uuid uuid_{};
        std::string name_ {};
        entt::entity entity_handle_ {0};
        Scene* scene_ {nullptr};
        GameObject* parent_ {nullptr};
        std::vector<Scope<GameObject>> children_ {};
    };
} // dodoe

#include "scene.h"

namespace dodoe {
    template <typename T, typename... Args>
    T& GameObject::add_component(Args&&... args) {
        return scene_->registry_.get_or_emplace<T>(entity_handle_, std::forward<Args>(args)...);
    }

    template <typename T>
    T& GameObject::get_component() {
        return scene_->registry_.get<T>(entity_handle_);
    }

    template <typename T>
    bool GameObject::has_component() const {
        return scene_->registry_.all_of<T>(entity_handle_);
    }

    template <typename T>
    void GameObject::remove_component() const {
        scene_->registry_.remove<T>(entity_handle_);
    }
} // dodoe

#endif //DODOE_GAME_OBJECT_H
