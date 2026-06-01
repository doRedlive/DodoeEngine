// do@Redlive

#include "dockspace_layout_builder.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {

	namespace {

		Bool StageMatches(EditorPanelStage panel_stage, EditorPanelStage active_stage) {
			return panel_stage == EditorPanelStage::Shared || panel_stage == active_stage;
		}

	}

	void DockspaceLayoutBuilder::buildDefaultLayout(const ImGuiID dockspace_id,
		const ImGuiViewport* viewport,
		const DynamicArray<const EditorPanel*>& panels,
		const EditorPanelStage active_stage) const {
		if (dockspace_id == 0 || viewport == nullptr) {
			return;
		}

		float left_ratio = 0.20f;
		float right_ratio = 0.24f;
		float bottom_ratio = 0.30f;

		for (const auto& panel : panels) {
			const auto& descriptor = panel->getDescriptor();
			if (!StageMatches(descriptor.stage, active_stage)) {
				continue;
			}

			switch (descriptor.default_dock.placement) {
				case EditorDockPlacement::Left:
					left_ratio = descriptor.default_dock.split_ratio;
					break;
				case EditorDockPlacement::Right:
					right_ratio = descriptor.default_dock.split_ratio;
					break;
				case EditorDockPlacement::Bottom:
					bottom_ratio = descriptor.default_dock.split_ratio;
					break;
				default:
					break;
			}
		}

		ImGui::DockBuilderRemoveNode(dockspace_id);
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

		ImGuiID dock_center = dockspace_id;
		const ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Left, left_ratio, nullptr, &dock_center);
		const ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, right_ratio, nullptr, &dock_center);
		const ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, bottom_ratio, nullptr, &dock_center);

		// Split left dock into top (Scene) and bottom (FileSystem)
		ImGuiID dock_left_top = dock_left;
		ImGuiID dock_left_bottom = dock_left;
		Int32 left_panel_count = 0;
		for (const auto& panel : panels) {
			const auto& descriptor = panel->getDescriptor();
			if (!StageMatches(descriptor.stage, active_stage) || descriptor.title.empty()) {
				continue;
			}
			if (descriptor.default_dock.placement == EditorDockPlacement::Left) {
				++left_panel_count;
			}
		}
		if (left_panel_count > 1) {
			dock_left_top = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.50f, nullptr, &dock_left_bottom);
		}

		// Dock windows to their nodes
		Int32 left_index = 0;
		for (const auto& panel : panels) {
			const auto& descriptor = panel->getDescriptor();
			if (!StageMatches(descriptor.stage, active_stage)) {
				continue;
			}
			if (descriptor.title.empty()) {
				continue;
			}

			switch (descriptor.default_dock.placement) {
				case EditorDockPlacement::Left:
					if (left_panel_count > 1 && left_index == 0) {
						ImGui::DockBuilderDockWindow(descriptor.title.c_str(), dock_left_top);
					} else {
						ImGui::DockBuilderDockWindow(descriptor.title.c_str(), dock_left_bottom);
					}
					++left_index;
					break;
				case EditorDockPlacement::Right:
					ImGui::DockBuilderDockWindow(descriptor.title.c_str(), dock_right);
					break;
				case EditorDockPlacement::Bottom:
					ImGui::DockBuilderDockWindow(descriptor.title.c_str(), dock_bottom);
					break;
				case EditorDockPlacement::Center:
					ImGui::DockBuilderDockWindow(descriptor.title.c_str(), dock_center);
					break;
				case EditorDockPlacement::None:
				default:
					break;
			}
		}

		ImGui::DockBuilderFinish(dockspace_id);
	}

} // cakery
