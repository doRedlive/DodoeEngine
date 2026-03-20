//
// Created by GreenMuffin on 2025/12/9.
//

#include "scene_hierarchy_cmd.h"

#include "cakery/panels/hierarchy_panel.h"

#include "runtime/function/context.h"

namespace cakery {
    SceneHierarchyHandler::SceneHierarchyHandler(HierarchyPanel& context) : hierarchy_panel_(context) {}

    SceneHierarchyHandler::~SceneHierarchyHandler() = default;

    void SceneHierarchyHandler::CreateGameObjectCmd::execute() {
        const std::string game_object_name = std::string("New GameObject ") + std::to_string(context_.hierarchy_panel_.context_->get_all_game_objects_count());
        context_.create_game_object_(game_object_name);
        game_object_name_ = game_object_name;
    }

    void SceneHierarchyHandler::CreateGameObjectCmd::undo() {
        const auto game_object = context_.hierarchy_panel_.context_->get_game_object(game_object_name_);
        context_.delete_game_object_(game_object);
    }

    void SceneHierarchyHandler::DeleteGameObjectCmd::execute() {
        context_.delete_game_object_(game_object_);
    }

    void SceneHierarchyHandler::DeleteGameObjectCmd::undo() {
        if (const auto game_object = context_.hierarchy_panel_.context_->get_game_object(game_object_->get_name()); !game_object) {
            context_.create_game_object_(game_object_->get_name());
            DoDebug("SceneHierarchy: restored '{}' ptr={}", game_object_->get_name(), static_cast<void*>(game_object_));
        }
    }

    void SceneHierarchyHandler::CreateChildGameObjectCmd::execute() {
        chile_game_object_name_ = game_object_->add_child()->get_name();
    }

    void SceneHierarchyHandler::CreateChildGameObjectCmd::undo() {
        if (const auto child_game_object = game_object_->get_child(chile_game_object_name_)) {
            context_.delete_game_object_(child_game_object);
        }
    }

    void SceneHierarchyHandler::create_game_object_(const std::string &name) const {
        hierarchy_panel_.context_->create_game_object(name);
    }

    void SceneHierarchyHandler::delete_game_object_(dodoe::GameObject *game_object) const {
        DoDebug("SceneHierarchy: deleting '{}' ptr={}", game_object->get_name(), static_cast<void*>(game_object));
        if (game_object->has_parent()) {
            DoError("delete child game object {} from parent", game_object->get_name());
            game_object->get_parent()->remove_child(game_object->get_name());
        }
        else {
            DoError("delete scene root game object {} from parent", game_object->get_name());
            hierarchy_panel_.context_->destroy_game_object(game_object->get_name());       // game_object 被删除后不能再访问
        }

        if (hierarchy_panel_.selected_game_object_ == game_object) {
            DoDebug("SceneHierarchy: cleared selection because deleted object was selected: {} ptr={}", game_object->get_name(), static_cast<void*>(game_object));
            hierarchy_panel_.selected_game_object_ = nullptr;
        }
    }

}
