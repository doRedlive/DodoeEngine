//
// Created by GreenMuffin on 2025/12/7.
//

#ifndef CAKERY_HIERARCHY_PANEL_H
#define CAKERY_HIERARCHY_PANEL_H

#include "cakery/command/scene_hierarchy_cmd.h"

namespace dodoe {
    class GameObject;
    class Scene;
}

namespace cakery {
    class HierarchyPanel {
        friend class SceneHierarchyHandler;
    public:
        HierarchyPanel();
        ~HierarchyPanel() = default;

        void on_ui_render();

        void set_context(dodoe::Scene* context);

    private:
        dodoe::Scene* context_ {nullptr};
        dodoe::GameObject* selected_game_object_ {nullptr};

        SceneHierarchyHandler handler_;

        void draw_game_object_node_(dodoe::GameObject* game_object);
    };
}


#endif//CAKERY_HIERARCHY_PANEL_H
