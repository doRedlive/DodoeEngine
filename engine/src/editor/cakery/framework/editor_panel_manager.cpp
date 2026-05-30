#include "editor_panel_manager.h"

#include <algorithm>
#include <unordered_map>

#include "imgui/imgui.h"

namespace cakery {

    void EditorPanelManager::registerPanel(std::unique_ptr<EditorPanel> panel) {
        if (!panel) {
            return;
        }

        const std::string id = panel->getDescriptor().id;
        if (id.empty() || m_panels_by_id.contains(id)) {
            return;
        }

        m_panels_by_id.emplace(id, panel.get());
        m_panels.push_back(std::move(panel));
        sortPanels();
    }

    void EditorPanelManager::initialize(const EditorPanelContext& context) {
        if (m_initialized) {
            return;
        }

        for (const auto& panel : m_panels) {
            panel->onAttach(context);
        }

        m_initialized = true;
        m_layout_dirty = true;
    }

    void EditorPanelManager::shutdown(const EditorPanelContext& context) {
        if (!m_initialized) {
            return;
        }

        if (m_workspace_active) {
            setWorkspaceActive(false, context);
        }

        for (auto it = m_panels.rbegin(); it != m_panels.rend(); ++it) {
            (*it)->onDetach(context);
        }

        m_initialized = false;
        m_dockspace_id = 0;
        m_layout_dirty = true;
    }

    void EditorPanelManager::setWorkspaceActive(const bool active, const EditorPanelContext& context) {
        if (m_workspace_active == active) {
            return;
        }

        m_workspace_active = active;
        if (active) {
            m_layout_dirty = true;
            for (const auto& panel : m_panels) {
                panel->onWorkspaceActivated(context);
            }
        }
        else {
            for (auto it = m_panels.rbegin(); it != m_panels.rend(); ++it) {
                (*it)->onWorkspaceDeactivated(context);
            }
        }
    }

    void EditorPanelManager::update(const EditorPanelStage stage, const EditorPanelContext& context, const float delta_time) {
        for (const auto& panel : m_panels) {
            if (!shouldProcessPanel(*panel, context, stage)) {
                continue;
            }

            panel->onUpdate(context, delta_time);
        }
    }

    void EditorPanelManager::draw(const EditorPanelStage stage, const EditorPanelContext& context) {
        for (const auto& panel : m_panels) {
            if (!shouldProcessPanel(*panel, context, stage)) {
                continue;
            }

            panel->onDraw(context);

            const ImGuiID dockspace_id = panel->getDockspaceId();
            if (dockspace_id != 0) {
                m_dockspace_id = dockspace_id;
                if (stage == EditorPanelStage::Workspace && m_layout_dirty) {
                    applyDefaultLayout(stage);
                }
            }
        }
    }

    void EditorPanelManager::drawViewMenuItems() {
        if (ImGui::MenuItem("Reset Layout")) {
            requestLayoutReset();
        }
        ImGui::Separator();

        std::unordered_map<std::string, std::vector<EditorPanel*>> by_category{};
        std::vector<std::string> category_order{};

        for (const auto& panel : m_panels) {
            const auto& descriptor = panel->getDescriptor();
            if (!descriptor.show_in_view_menu || !stageMatches(descriptor.stage, EditorPanelStage::Workspace)) {
                continue;
            }
            if (descriptor.requires_runtime && !m_workspace_active) {
                continue;
            }

            std::string category = descriptor.category.empty() ? "Panels" : descriptor.category;
            if (!by_category.contains(category)) {
                category_order.push_back(category);
            }
            by_category[category].push_back(panel.get());
        }

        for (const auto& category : category_order) {
            if (!ImGui::BeginMenu(category.c_str())) {
                continue;
            }

            for (const auto& panel : by_category[category]) {
                const auto& descriptor = panel->getDescriptor();

                if (!descriptor.closable) {
                    ImGui::TextDisabled("%s", descriptor.title.c_str());
                    continue;
                }

                bool open = panel->isOpen();
                if (ImGui::MenuItem(descriptor.title.c_str(), nullptr, &open)) {
                    panel->setOpen(open);
                }
            }

            ImGui::EndMenu();
        }
    }

    void EditorPanelManager::requestLayoutReset() {
        m_layout_dirty = true;
    }

    EditorPanel* EditorPanelManager::findPanel(const std::string& id) const {
        const auto it = m_panels_by_id.find(id);
        return it != m_panels_by_id.end() ? it->second : nullptr;
    }

    bool EditorPanelManager::stageMatches(const EditorPanelStage panel_stage, const EditorPanelStage active_stage) const {
        return panel_stage == EditorPanelStage::Shared || panel_stage == active_stage;
    }

    bool EditorPanelManager::panelRequiresRuntime(const EditorPanelDescriptor& descriptor, const EditorPanelContext& context) const {
        if (!descriptor.requires_runtime) {
            return false;
        }
        return !context.workspace_active;
    }

    bool EditorPanelManager::shouldProcessPanel(const EditorPanel& panel, const EditorPanelContext& context, const EditorPanelStage stage) const {
        const auto& descriptor = panel.getDescriptor();
        if (!stageMatches(descriptor.stage, stage)) {
            return false;
        }
        if (panelRequiresRuntime(descriptor, context)) {
            return false;
        }
        if (!panel.isOpen() && descriptor.closable) {
            return false;
        }
        return true;
    }

    void EditorPanelManager::sortPanels() {
        std::ranges::sort(m_panels, [](const std::unique_ptr<EditorPanel>& lhs, const std::unique_ptr<EditorPanel>& rhs) {
            return lhs->getDescriptor().order < rhs->getDescriptor().order;
        });
    }

    void EditorPanelManager::applyDefaultLayout(const EditorPanelStage stage) {
        if (m_dockspace_id == 0) {
            return;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport) {
            return;
        }

        m_layout_builder.buildDefaultLayout(m_dockspace_id, viewport, m_panels, stage);
        m_layout_dirty = false;
    }

} // cakery
