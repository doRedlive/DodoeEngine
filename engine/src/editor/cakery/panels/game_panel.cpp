// do@Redlive

#include "game_panel.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {

	GamePanel::GamePanel(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	GamePanel::~GamePanel() {
	}

	void GamePanel::onWorkspaceActivated(const EditorPanelContext& context) {
		(void)context;
	}

	void GamePanel::onWorkspaceDeactivated(const EditorPanelContext& context) {
		(void)context;
	}

	void GamePanel::onUpdate(const EditorPanelContext& context, const float delta_time) {
		(void)context;
		(void)delta_time;
	}

	void GamePanel::onDraw(const EditorPanelContext& context) {
		(void)context;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("Game", nullptr, ImGuiWindowFlags_NoBackground);

		// Toolbar overlay
		{
			ImGui::SetCursorPos(ImVec2(4.0f, 4.0f));
			ImGui::BeginGroup();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));

			ImGui::TextUnformatted("Game");

			ImGui::PopStyleVar(2);
			ImGui::EndGroup();
		}

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		m_game_content_size.x = (std::max)(1.0f, avail.x);
		m_game_content_size.y = (std::max)(1.0f, avail.y);
		ImGui::InvisibleButton("GameCanvas", ImVec2(m_game_content_size.x, m_game_content_size.y));

		// Emit a cyan mask color for Game view so CombinePass can distinguish from Scene
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 mask_min = ImGui::GetItemRectMin();
		const ImVec2 mask_max = ImGui::GetItemRectMax();
		draw_list->AddRectFilled(mask_min, mask_max, IM_COL32(0, 255, 255, 255));

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

} // cakery
