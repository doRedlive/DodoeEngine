#include "editor_panel.h"

namespace cakery {

    EditorPanel::EditorPanel(EditorPanelDescriptor descriptor)
        : m_descriptor(std::move(descriptor)), m_is_open(m_descriptor.default_open) {
    }

    void EditorPanel::onAttach(const EditorPanelContext& context) {
        (void)context;
    }

    void EditorPanel::onDetach(const EditorPanelContext& context) {
        (void)context;
    }

    void EditorPanel::onWorkspaceActivated(const EditorPanelContext& context) {
        (void)context;
    }

    void EditorPanel::onWorkspaceDeactivated(const EditorPanelContext& context) {
        (void)context;
    }

    void EditorPanel::onUpdate(const EditorPanelContext& context, const float delta_time) {
        (void)context;
        (void)delta_time;
    }

    ImGuiID EditorPanel::getDockspaceId() const {
        return 0;
    }

} // namespace cakery
