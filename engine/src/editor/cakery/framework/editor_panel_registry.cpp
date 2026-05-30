#include "editor_panel_registry.h"

#include "editor_panel_manager.h"

namespace cakery {

    EditorPanelRegistry& EditorPanelRegistry::Self() {
        static EditorPanelRegistry registry;
        return registry;
    }

    bool EditorPanelRegistry::registerPanel(EditorPanelDescriptor descriptor, EditorPanelFactory factory) {
        if (descriptor.id.empty() || !factory || m_entries.contains(descriptor.id)) {
            return false;
        }

        const std::string id = descriptor.id;
        m_order.push_back(id);
        m_entries.emplace(id, PanelEntry{std::move(descriptor), std::move(factory)});
        return true;
    }

    void EditorPanelRegistry::instantiatePanels(EditorPanelManager& manager) const {
        for (const std::string& id : m_order) {
            const auto it = m_entries.find(id);
            if (it == m_entries.end()) {
                continue;
            }

            auto panel = it->second.factory();
            if (panel) {
                manager.registerPanel(std::move(panel));
            }
        }
    }

} // cakery
