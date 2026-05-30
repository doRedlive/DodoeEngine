// do@Redlive

#pragma once

#include "cakery/framework/editor_panel.h"
#include "imgui/imgui.h"

namespace cakery {

    class DockspacePanel : public EditorPanel {
    public:
        explicit DockspacePanel(EditorPanelDescriptor descriptor);
        void onDraw(const EditorPanelContext& context) override;
        [[nodiscard]] ImGuiID getDockspaceId() const override { return m_dockspace_id; }

    private:
        ImGuiID m_dockspace_id{0};
    };

} // cakery
