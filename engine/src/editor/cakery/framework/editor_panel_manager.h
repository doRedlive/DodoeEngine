#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "dockspace_layout_builder.h"
#include "editor_panel.h"

namespace cakery {

    class EditorPanelManager {
    public:
        void registerPanel(std::unique_ptr<EditorPanel> panel);

        void initialize(const EditorPanelContext& context);
        void shutdown(const EditorPanelContext& context);

        void setWorkspaceActive(bool active, const EditorPanelContext& context);
        void update(EditorPanelStage stage, const EditorPanelContext& context, float delta_time);
        void draw(EditorPanelStage stage, const EditorPanelContext& context);

        void drawViewMenuItems();
        void requestLayoutReset();

        [[nodiscard]] EditorPanel* findPanel(const std::string& id) const;

    private:
        bool stageMatches(EditorPanelStage panel_stage, EditorPanelStage active_stage) const;
        bool panelRequiresRuntime(const EditorPanelDescriptor& descriptor, const EditorPanelContext& context) const;
        bool shouldProcessPanel(const EditorPanel& panel, const EditorPanelContext& context, EditorPanelStage stage) const;
        void sortPanels();
        void applyDefaultLayout(EditorPanelStage stage);

        std::vector<std::unique_ptr<EditorPanel>> m_panels{};
        std::unordered_map<std::string, EditorPanel*> m_panels_by_id{};
        DockspaceLayoutBuilder m_layout_builder{};
        ImGuiID m_dockspace_id{0};
        bool m_initialized{false};
        bool m_workspace_active{false};
        bool m_layout_dirty{true};
    };

}
