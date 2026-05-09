//
// Created by GreenMuffin on 2025/12/7.
//

#ifndef CAKERY_CAKERY_LAYER_H
#define CAKERY_CAKERY_LAYER_H

#include "panels/console_panel.h"
#include "panels/inspector_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/project_panel.h"
#include "panels/viewport_panel.h"
#include "panels/dockspace_panel.h"
#include "panels/title_bar.h"

#include "runtime/core/layer/layer.h"
#include "runtime/function/window/window.h"

namespace cakery {
    class CakeryLayer final : public dodoe::Layer {
    public:
        explicit CakeryLayer(const std::string& name);
        ~CakeryLayer() override;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;

    private:
        dodoe::Window* cakery_window_{ nullptr };
        bool simulation_frame_context_registered_{false};
        bool scene_light_created_{false};
        float fps_accumulated_time_{0.0f};
        uint32_t fps_frame_counter_{0};
        std::string base_window_title_{};

        HierarchyPanel hierarchy_panel_{};
        ProjectPanel project_panel_{};
        InspectorPanel inspector_panel_{};
        ConsolePanel console_panel_{};
        DockSpacePanel dockspace_panel_{};
        ViewportPanel viewport_panel_;
        Titlebar title_bar_{};
    };
} // cakery


#endif //CAKERY_CAKERY_LAYER_H
