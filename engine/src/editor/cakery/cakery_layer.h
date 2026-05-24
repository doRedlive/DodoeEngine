// do@Redlive

#pragma once

#include "panels/console_panel.h"
#include "panels/inspector_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/project_panel.h"
#include "panels/project_manager_panel.h"
#include "panels/viewport_panel.h"
#include "panels/dockspace_panel.h"
#include "panels/title_bar.h"

#include "runtime/core/layer/layer.h"
#include "runtime/function/window/window.h"

namespace cakery {
    class CakeryLayer final : public dodoe::Layer {
    public:
        explicit CakeryLayer(const std::string& name);
        ~CakeryLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;

    private:
        void enterEditor();

        dodoe::Window* m_window{ nullptr };
        std::string m_base_title{};
        bool m_editor_initialized{false};

        ProjectManagerPanel m_project_manager_panel{};
        HierarchyPanel m_hierarchy_panel{};
        ProjectPanel m_project_panel{};
        InspectorPanel m_inspector_panel{};
        ConsolePanel m_console_panel{};
        DockSpacePanel m_dockspace_panel{};
        ViewportPanel m_viewport_panel;
        Titlebar m_title_bar{};
    };
} // cakery
