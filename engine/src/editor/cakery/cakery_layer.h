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

#include "command/command_handler.h"

#include "runtime/core/application.h"
#include "runtime/core/layer/layer.h"
#include "runtime/function/window/window.h"

namespace cakery {
    class CakeryLayer final : public dodoe::Layer {
    public:
        explicit CakeryLayer(const std::string& name);
        ~CakeryLayer() override;

        void on_attach() override;
        void on_detach() override;
        void on_update(float delta_time) override;
        void on_ui_render() override;

    private:
        dodoe::Window* cakery_window_{ nullptr };

        bool is_custom_titlebar_{ true };

        HierarchyPanel hierarchy_panel_{};
        ProjectPanel project_panel_{};
        InspectorPanel inspector_panel_{};
        ConsolePanel console_panel_{};
        //ViewportPanel viewport_panel_{};
    };
} // cakery


#endif //CAKERY_CAKERY_LAYER_H