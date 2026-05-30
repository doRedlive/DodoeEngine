#pragma once

#include <memory>
#include <vector>

#include "editor_panel.h"

struct ImGuiViewport;

typedef unsigned int ImGuiID;

namespace cakery {

    class EditorPanel;

    class DockspaceLayoutBuilder {
    public:
        void buildDefaultLayout(ImGuiID dockspace_id,
            const ImGuiViewport* viewport,
            const std::vector<std::unique_ptr<EditorPanel>>& panels,
            EditorPanelStage active_stage) const;
    };

}
