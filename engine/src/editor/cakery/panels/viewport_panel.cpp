// do@Redlive

#include "viewport_panel.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/framework/camera.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

using namespace dodoe;

namespace cakery {

	ViewportPanel::ViewportPanel(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	ViewportPanel::~ViewportPanel() {
	}

	void ViewportPanel::onWorkspaceActivated(const EditorPanelContext& context) {
		(void)context;
	}

	void ViewportPanel::onWorkspaceDeactivated(const EditorPanelContext& context) {
		(void)context;
	}

	void ViewportPanel::onUpdate(const EditorPanelContext& context, const float delta_time) {
		(void)context;
		(void)delta_time;
	}

	void ViewportPanel::drawGameCameraRect(ImDrawList*          draw_list,
	                                       const ImVec2&        vp_min,
	                                       const ImVec2&        vp_max,
	                                       const Camera&        camera) const {
		const float half_w = m_game_camera_size.x * 0.5f;
		const float half_h = m_game_camera_size.y * 0.5f;

		const float gx = m_game_camera_position.x;
		const float gy = m_game_camera_position.y;

		const Vector3f world_corners[4] = {
			{gx - half_w, gy + half_h, 0.0f},
			{gx + half_w, gy + half_h, 0.0f},
			{gx + half_w, gy - half_h, 0.0f},
			{gx - half_w, gy - half_h, 0.0f},
		};

		const auto& logical_size = camera.getLogicalSize();
		const float panel_w = vp_max.x - vp_min.x;
		const float panel_h = vp_max.y - vp_min.y;
		const float scale_x = panel_w / (logical_size.x > 0.0f ? logical_size.x : 1.0f);
		const float scale_y = panel_h / (logical_size.y > 0.0f ? logical_size.y : 1.0f);

		auto to_screen = [&](const Vector3f& w) -> ImVec2 {
			const Vector3f s = camera.world2screen(w);
			return { vp_min.x + s.x * scale_x, vp_min.y + s.y * scale_y };
		};

		constexpr ImU32 kBorderColor = IM_COL32(139, 233, 253, 255);
		constexpr float kThickness   = 2.0f;
		for (int i = 0; i < 4; ++i) {
			const ImVec2 a = to_screen(world_corners[i]);
			const ImVec2 b = to_screen(world_corners[(i + 1) % 4]);
			draw_list->AddLine(a, b, kBorderColor, kThickness);
		}

		{
			const ImVec2 centre = to_screen({gx, gy, 0.0f});
			draw_list->AddLine(
				ImVec2(vp_min.x, centre.y),
				ImVec2(vp_max.x, centre.y),
				IM_COL32(255, 121, 198, 255), 1.5f);
		}

		{
			const ImVec2 centre = to_screen({gx, gy, 0.0f});
			draw_list->AddLine(
				ImVec2(centre.x, vp_min.y),
				ImVec2(centre.x, vp_max.y),
				IM_COL32(80, 250, 123, 255), 1.5f);
		}
	}

	void ViewportPanel::drawZoomBar(ImDrawList*    draw_list,
	                                const ImVec2&  vp_min,
	                                const ImVec2&  vp_max,
	                                Camera&         camera) {
		constexpr float kBarWidth  = 10.0f;
		constexpr float kBarHeight = 130.0f;
		constexpr float kMarginX   = 8.0f;
		constexpr float kMarginY   = 34.0f;

		const float bx = vp_min.x + kMarginX;
		const float by = vp_min.y + kMarginY;

		draw_list->AddRectFilled(
			ImVec2(bx, by),
			ImVec2(bx + kBarWidth, by + kBarHeight),
			IM_COL32(0, 0, 0, 255), 3.0f);

		const float zoom_pct = camera.getZoom() * 100.0f;
		constexpr float kLogMin = 0.0f;
		constexpr float kLogMax = 4.0f;
		const float log_zoom = std::log10((std::max)(zoom_pct, 1.0f));
		const float t = Math::Clamp((log_zoom - kLogMin) / (kLogMax - kLogMin), 0.0f, 1.0f);

		const float fill_h = t * kBarHeight;
		const float fy0 = by + kBarHeight - fill_h;
		draw_list->AddRectFilled(
			ImVec2(bx, fy0),
			ImVec2(bx + kBarWidth, by + kBarHeight),
			IM_COL32(80, 250, 123, 255), 3.0f);

		const float ticks[3] = { 1.0f, 2.0f, 3.0f };
		for (float tick_log : ticks) {
			const float tick_t = (tick_log - kLogMin) / (kLogMax - kLogMin);
			const float tick_y = by + kBarHeight - tick_t * kBarHeight;
			draw_list->AddLine(
				ImVec2(bx - 4.0f, tick_y),
				ImVec2(bx + kBarWidth * 0.5f, tick_y),
				IM_COL32(120, 120, 120, 255),
				1.0f
			);
		}

		char zoom_text[32];
		if (zoom_pct >= 100.0f) {
			snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", zoom_pct);
		} else if (zoom_pct >= 10.0f) {
			snprintf(zoom_text, sizeof(zoom_text), "%.0f%%", zoom_pct);
		} else {
			snprintf(zoom_text, sizeof(zoom_text), "%.1f%%", zoom_pct);
		}
		const float text_y = Math::Clamp(fy0 + 4.0f, by + 2.0f, by + kBarHeight - 10.0f);
		draw_list->AddText(
			ImVec2(bx + kBarWidth + 5.0f, text_y),
			IM_COL32(255, 184, 108, 255),
			zoom_text
		);
	}

	void ViewportPanel::onDraw(const EditorPanelContext& context) {
		(void)context;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoBackground);

		const bool is_2d = Application::Self().context().render_system->getMainCamera().getCameraType() == CameraType::Orthographic;

		{
			ImGui::SetCursorPos(ImVec2(4.0f, 4.0f));
			ImGui::BeginGroup();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
			ImGui::TextUnformatted("Scene");
			ImGui::PopStyleVar(2);
			ImGui::EndGroup();
		}

		const ImVec2 content_screen_pos = ImGui::GetCursorScreenPos();
		const ImVec2 main_viewport_pos   = ImGui::GetMainViewport()->Pos;
		const ImVec2 content_pos(
			content_screen_pos.x - main_viewport_pos.x,
			content_screen_pos.y - main_viewport_pos.y
		);
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		m_viewport_content_size.x = (std::max)(1.0f, avail.x);
		m_viewport_content_size.y = (std::max)(1.0f, avail.y);
		Application::Self().context().render_system->getViewportManager()->setViewportRect(
			Rect(Vector2f(content_pos.x, content_pos.y), m_viewport_content_size)
		);

		const ImVec2 vp_min_pre = content_screen_pos;
		const ImVec2 vp_max_pre = ImVec2(
			vp_min_pre.x + m_viewport_content_size.x,
			vp_min_pre.y + m_viewport_content_size.y
		);

		auto& camera = Application::Self().context().render_system->getMainCamera();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();

		if (is_2d) {
			constexpr float kBarWidth  = 10.0f;
			constexpr float kBarHeight = 130.0f;
			constexpr float kMarginX   = 8.0f;
			constexpr float kMarginY   = 34.0f;

			const float bar_x = vp_min_pre.x + kMarginX;
			const float bar_y = vp_min_pre.y + kMarginY;

			ImGui::SetCursorScreenPos(ImVec2(bar_x, bar_y));
			ImGui::InvisibleButton("##ZoomBar", ImVec2(kBarWidth, kBarHeight));

			if (ImGui::IsItemActive()) {
				const float mouse_y = ImGui::GetIO().MousePos.y;
				float local_t = 1.0f - (mouse_y - bar_y) / kBarHeight;
				local_t = Math::Clamp(local_t, 0.0f, 1.0f);

				constexpr float kLogMin = 0.0f;
				constexpr float kLogMax = 4.0f;
				const float new_log_zoom = kLogMin + local_t * (kLogMax - kLogMin);
				const float new_zoom_pct = std::pow(10.0f, new_log_zoom);
				camera.setZoom(new_zoom_pct / 100.0f);
			}
		}

		ImGui::SetCursorScreenPos(content_screen_pos);
		ImGui::InvisibleButton("ViewportCanvas", ImVec2(m_viewport_content_size.x, m_viewport_content_size.y));

		const ImVec2 vp_min = ImGui::GetItemRectMin();
		const ImVec2 vp_max = ImGui::GetItemRectMax();

		if (is_2d) {
			drawGameCameraRect(draw_list, vp_min, vp_max, camera);
		}

		if (is_2d) {
			drawZoomBar(draw_list, vp_min, vp_max, camera);
		}

		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

} // cakery
