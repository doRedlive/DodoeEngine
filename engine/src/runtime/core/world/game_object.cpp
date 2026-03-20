//
// Created by GreenMuffin on 2025/11/15.
//

#include "game_object.h"
#include "components.h"
#include "entity.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/context.h"

namespace dodoe {

    GameObject::GameObject() = default;

    GameObject::GameObject(const entt::entity entity, Scene *scene) : entity_handle_(entity), scene_(scene) {

    }

    GameObject::~GameObject() = default;

    const Uuid& GameObject::uuid() const {
        return uuid_;
    }

    const std::string &GameObject::get_name() const {
        return name_;
    }

    void GameObject::set_name(const std::string& name) {
        name_ = name;
    }

    bool GameObject::has_parent() const {
        return parent_ != nullptr;
    }

    GameObject *GameObject::get_parent() const {
        return parent_;
    }

    GameObject* GameObject::add_child(const std::string& name) {
        if (!scene_) {
            DoError("Cannot add child to GameObject '{}', because its scene is null.", name_);
            return nullptr;
        }
        const auto id = entt::hashed_string(name.c_str());
        for (const auto& child : children_) {
            if (child->index_ == id) {
                DoError("Child GameObject with name '{}' already exists under parent GameObject '{}'. Returning existing child.", name, name_);
                return child.get();
            }
        }
        auto child_entity = scene_->registry_.create();
        auto child = create_scope<GameObject>(registry_entity_handle(child_entity), scene_);
        child->index_ = id;
        child->name_ = name;
        child->add_component<TransformComponent>();
        child->add_component<TagComponent>();
        child->parent_ = this;
        children_.push_back(std::move(child));
        return get_child(name);
    }

    GameObject *GameObject::add_child() {
        const auto name = "New Child " + std::to_string(static_cast<int>(children_.size()));
        return add_child(name);
    }

    GameObject* GameObject::get_child(const std::string& name) const {
        const auto id = entt::hashed_string(name.c_str());
        for (const auto& child : children_) {
            if (child->index_ == id) {
                return child.get();
            }
        }
        DoError("No such child '{}' found.", name);
        return nullptr;
    }

    std::vector<GameObject*> GameObject::get_children() const {
        std::vector<GameObject*> children;
        children.reserve(children_.size());
        for (const auto& child : children_) {
            children.push_back(child.get());
        }
        return children;
    }

    void GameObject::remove_child(const std::string& name) {
        const auto id = entt::hashed_string(name.c_str());
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if ((*it)->index_ == id) {
                (*it)->destroy();
                children_.erase(it);
                return;
            }
        }
        DoError("No such child '{}' found to remove.", name);
    }

    void GameObject::remove_all_children() {
        for (const auto& child : children_) {
            if (child) {
                child->destroy();
            }
        }
        children_.clear();
    }

    void GameObject::destroy() {
        remove_all_children();
        if (!scene_) {
            DoError("Cannot destroy GameObject '{}' because scene is null.", name_);
            return;
        }
        if (!scene_->registry_.valid(entity_handle_)) {
            return;
        }
        scene_->registry_.destroy(entity_handle_);
        entity_handle_ = entt::null;
    }

    bool GameObject::has_component(const std::string &name) const {
        if (!scene_->registry_.valid(entity_handle_)) {
            DoError("Invalid entity handle for GameObject '{}'.", name_);
            return false;
        }
        for (auto&& [id, storage] : scene_->registry_.storage()) {
            if (!storage.contains(entity_handle_)) {
                continue;
            }
            auto type_name = std::string(storage.type().name());
            name_remove_namespace(type_name);
            if (type_name == name) {
                return true;
            }
        }
        return false;
    }


    void GameObject::add_component(const std::string& comp_name) {
        auto type_name = comp_name;
        name_remove_namespace(type_name);

        if (has_component(type_name)) {
            DoError("GameObject '{}' already has component '{}'.", name_, type_name);
            return;
        }

        auto comp_type_opt = ComponentDB::instance().type_from_name(comp_name);
        if (!comp_type_opt) {
            DoError("No this comp {}", comp_name);
            return;
        }

        auto add_func = ComponentDB::instance().add_component_func(type_name);
        if (!add_func) {
            DoError("No this comp {}", comp_name);
            return;
        }
        add_func(scene_->registry_.raw(), entity_handle_);
    }

    std::vector<entt::type_info> GameObject::get_all_components() const {
        std::vector<entt::type_info> result;
        if (!scene_->registry_.valid(entity_handle_)) {
            return result;
        }
        for (auto&& [id, storage] : scene_->registry_.storage()) {
            if (storage.contains(entity_handle_)) {
                result.emplace_back(storage.type());
            }
        }
        return result;
    }

    void* GameObject::get_component_value_ptr(const std::string& comp_name) const {
        if (!scene_->registry_.valid(entity_handle_)) {
            return nullptr;
        }
        for (auto&& [id, storage] : scene_->registry_.storage()) {
            if (!storage.contains(entity_handle_)) {
                continue;
            }
            auto type_name = std::string(storage.type().name());
            name_remove_namespace(type_name);
            if (type_name == comp_name) {
                return storage.value(entity_handle_);
            }
        }
        return nullptr;
    }
    
    void GameObject::remove_component(const std::string& comp_name) const {
        if (!scene_->registry_.valid(entity_handle_)) {
            return;
        }
        for (auto&& [id, storage] : scene_->registry_.storage()) {
            if (!storage.contains(entity_handle_)) {
                continue;
            }
            auto type_name = std::string(storage.type().name());
            name_remove_namespace(type_name);
            if (type_name == comp_name) {
                storage.remove(entity_handle_);
                return;
            }
        }
    }
} // dodoe
