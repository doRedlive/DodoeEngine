//
// Created by GreenMuffin on 2026/2/22.
//

#include "viewport_panel.h"

#include "runtime/function/render/render_api.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {
	ViewportPanel::~ViewportPanel() {
		cleanup();
	}

	void ViewportPanel::initialize() {
	}

	void ViewportPanel::update() {
	}

	void ViewportPanel::draw() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBackground);
		const ImVec2 content_screen_pos = ImGui::GetCursorScreenPos();
		const ImVec2 main_viewport_pos = ImGui::GetMainViewport()->Pos;
		const ImVec2 content_pos(
			content_screen_pos.x - main_viewport_pos.x,
			content_screen_pos.y - main_viewport_pos.y
		);
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		viewport_content_size_.x = (std::max)(1.0f, avail.x);
		viewport_content_size_.y = (std::max)(1.0f, avail.y);
		Application::Self().context().render_system->getViewportManager()->setViewportRect(
			Rect(
				Vector2f(content_pos.x, content_pos.y),
				viewport_content_size_
			)
		);
		ImGui::InvisibleButton("ViewportCanvas", ImVec2(viewport_content_size_.x, viewport_content_size_.y));

		// Emit a viewport mask color into the ImGui render target so CombinePass can replace only this region with scene color.
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 mask_min = ImGui::GetItemRectMin();
		const ImVec2 mask_max = ImGui::GetItemRectMax();
		draw_list->AddRectFilled(mask_min, mask_max, IM_COL32(255, 0, 255, 255));

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

	void ViewportPanel::cleanup() {
	}

} // cakery
