////
//// Created by GreenMuffin on 2025/12/12.
////
//
//#ifndef CAKERY_INSPECTOR_PANEL_H
//#define CAKERY_INSPECTOR_PANEL_H
//
//#include "dopch.h"
//
//#include "runtime/function/world/entity.h"
//
//namespace cakery {
//    class InspectorPanel {
//        std::unordered_map<std::string, std::function<void(std::string, void*)>> component_ui_drawer_ {};
//        std::vector<std::pair<std::string, bool>> node_state_array {};
//        int node_depth_ {-1};
//    public:
//        InspectorPanel();
//        void draw();
//
//    private:
//        void initialize_component_drawers_();
//        void draw_components_();
//        void draw_node_ui_(const std::string& comp_name);
//        void draw_buttons_(dodoe::Entity entity) const;
//    };
//} // cakery
//
//
//#endif//CAKERY_INSPECTOR_PANEL_H
