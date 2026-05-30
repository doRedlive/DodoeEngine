// do@Redlive

#pragma once

#include "dopch.h"

#include "editor_panel.h"

namespace cakery {

    class EditorPanel;
    class EditorPanelManager;

    using EditorPanelFactory = std::function<dodoe::Scope<EditorPanel>()>;

    class EditorPanelRegistry {
    public:
        static EditorPanelRegistry& Self();

        bool registerPanel(EditorPanelDescriptor descriptor, EditorPanelFactory factory);
        void instantiatePanels(EditorPanelManager& manager) const;

    private:
        struct PanelEntry {
            EditorPanelDescriptor descriptor{};
            EditorPanelFactory factory{};
        };

        std::vector<std::string> m_order{};
        std::unordered_map<std::string, PanelEntry> m_entries{};
    };

} // cakery
