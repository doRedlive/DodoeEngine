// do@Redlive

#include "dockspace_panel.h"

#include "imgui/imgui.h"

namespace cakery {

DockspacePanel::DockspacePanel(EditorPanelDescriptor descriptor)
	: EditorPanel(std::move(descriptor)) {
}

void DockspacePanel::onDraw(const EditorPanelContext& context) {
	(void)context;
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	// Reserve space for menu bar + toolbar
	constexpr float kToolbarHeight = 48.0f;
	constexpr float kToolbarMargin = 6.0f;
	constexpr float kSideMargin = 8.0f;
	const float toolbar_total = kToolbarHeight + kToolbarMargin * 2.0f;
	ImVec2 dock_pos(viewport->WorkPos.x + kSideMargin, viewport->WorkPos.y + toolbar_total);
	ImVec2 dock_size(viewport->WorkSize.x - kSideMargin * 2.0f, viewport->WorkSize.y - toolbar_total - kToolbarMargin);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowPos(dock_pos);
	ImGui::SetNextWindowSize(dock_size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("##DockSpaceHost", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	m_dockspace_id = ImGui::GetID("DockSpaceHostID");
	ImGui::DockSpace(m_dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

} // cakery
