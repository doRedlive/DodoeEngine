//
// Created by GreenMuffin on 2025/12/12.
//

#ifndef CAKERY_INSPECTOR_PANEL_H
#define CAKERY_INSPECTOR_PANEL_H

#include "dopch.h"

#include <unordered_set>

#include "cakery/cakery_event.h"

#include "runtime/function/world/entity.h"

namespace cakery {
   namespace dodoe = ::dodoe;
}

namespace dodoe {
    class MonoComponentInstance;
    class ScriptClass;
}

namespace cakery {
   class InspectorPanel {
       std::unordered_map<std::string, std::function<void(std::string, void*)>> m_component_ui_drawer{};
       std::vector<std::pair<std::string, bool>> m_node_state_array {};
       int m_node_depth {-1};
       std::string m_current_component_name{};
       dodoe::Entity m_selected_entity;
   public:
       InspectorPanel();
       ~InspectorPanel();
       void draw();

   private:
       void initializeComponentDrawers();
       void drawComponents();
       void drawNode(const std::string& comp_name);
       void drawButtons(dodoe::Entity entity);
       void drawMonoComponents();
       void drawMonoNode(const std::string& comp_name, dodoe::MonoComponentInstance& component_instance, dodoe::ScriptClass& script_class);
       void markCurrentComponentDirty();

       void onSelectEntity(const SelectEntityEvent& evnet);
       void onNonSelectEntity();
   };
} // cakery


#endif//CAKERY_INSPECTOR_PANEL_H
