//
// Created by GreenMuffin on 2026/3/4.
//

#include "title_bar.h"

#include "cakery/style/style.h"

#include "runtime/function/context.h"
#include "runtime/function/window/window.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

#include "runtime/core/application.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {	

	void Titlebar::draw(dodoe::Window* cakery_window) {
		if (cakery_window == nullptr) {
			return;
		}

		GLFWwindow* native_window = cakery_window->native_window();
		if (native_window == nullptr) {
			return;
		}

		const float titlebar_height = 58.0f;
		const bool is_maximized = cakery_window->is_maximized();
		const float titlebar_vertical_offset = is_maximized ? -6.0f : 0.0f;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titlebar_vertical_offset));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titlebar_height));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		const ImGuiWindowFlags titlebar_flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoMove;

		if (!ImGui::Begin("##cakery_native_titlebar", nullptr, titlebar_flags)) {
			ImGui::End();
			ImGui::PopStyleVar(3);
			return;
		}

		const ImVec2 titlebar_min = ImGui::GetWindowPos();
		const ImVec2 titlebar_max = ImVec2(titlebar_min.x + ImGui::GetWindowWidth(), titlebar_min.y + titlebar_height);
		auto* bg_draw_list = ImGui::GetBackgroundDrawList();
		auto* fg_draw_list = ImGui::GetForegroundDrawList();
		bg_draw_list->AddRectFilled(titlebar_min, titlebar_max, Style::Colors::Theme::titlebar);
		bg_draw_list->AddLine(
			ImVec2(titlebar_min.x, titlebar_max.y - 1.0f),
			ImVec2(titlebar_max.x, titlebar_max.y - 1.0f),
			Style::Colors::Theme::muted,
			1.0f
		);

		const float buttons_area_width = 110.0f;
		const float w = ImGui::GetWindowWidth();
		const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

		// Left logo mark (similar placement to Walnut style titlebar)
		{
			const float logo_size = 24.0f;
			const ImVec2 logo_min(titlebar_min.x + 16.0f + window_padding.x, titlebar_min.y + 10.0f + window_padding.y + titlebar_vertical_offset);
			const ImVec2 logo_max(logo_min.x + logo_size, logo_min.y + logo_size);
			fg_draw_list->AddRectFilled(logo_min, logo_max, Style::Colors::Theme::accent, 6.0f);
			const ImVec2 text_size = ImGui::CalcTextSize("C");
			const ImVec2 text_pos((logo_min.x + logo_max.x) * 0.5f - text_size.x * 0.5f, (logo_min.y + logo_max.y) * 0.5f - text_size.y * 0.5f - 1.0f);
			fg_draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), "C");
		}

		ImGui::SetCursorPos(ImVec2(window_padding.x, window_padding.y + titlebar_vertical_offset));
		ImGui::InvisibleButton("##titleBarDragZone", ImVec2(ImMax((w > buttons_area_width) ? (w - buttons_area_width) : 0.0f, 1.0f), ImMax(titlebar_height, 1.0f)));

		bool titlebar_hovered = ImGui::IsItemHovered();
		if (is_maximized) {
			const float window_mouse_pos_y = ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y;
			if (window_mouse_pos_y >= 0.0f && window_mouse_pos_y <= 5.0f) {
				titlebar_hovered = true;
			}
		}
		cakery_window->set_titlebar_hovered(titlebar_hovered);

		// Centered title
		const char* title_text = dodoe::Application::self().specification().name.c_str();
		const ImVec2 text_size = ImGui::CalcTextSize(title_text);
		const ImVec2 text_pos(
			titlebar_min.x + (titlebar_max.x - titlebar_min.x) * 0.5f - text_size.x * 0.5f,
			titlebar_min.y + window_padding.y + 12.0f + titlebar_vertical_offset
		);
		fg_draw_list->AddText(text_pos, Style::Colors::Theme::text, title_text);

		auto multiplied_color = [](ImU32 color, float multiplier) -> ImU32 {
			ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
			c.x = ImClamp(c.x * multiplier, 0.0f, 1.0f);
			c.y = ImClamp(c.y * multiplier, 0.0f, 1.0f);
			c.z = ImClamp(c.z * multiplier, 0.0f, 1.0f);
			return ImGui::ColorConvertFloat4ToU32(c);
		};

		const ImU32 button_col_n = multiplied_color(Style::Colors::Theme::text, 0.9f);
		const ImU32 button_col_h = multiplied_color(Style::Colors::Theme::text, 1.2f);
		const ImU32 button_col_p = Style::Colors::Theme::textDarker;
		const float button_width = 14.0f;
		const float button_height = 14.0f;
		const float button_y = titlebar_min.y + 12.0f + window_padding.y + titlebar_vertical_offset;

		auto draw_button = [&](const char* id, float x, auto icon_draw) -> bool {
			ImGui::SetCursorScreenPos(ImVec2(x, button_y));
			ImGui::InvisibleButton(id, ImVec2(button_width, button_height));
			const bool hovered = ImGui::IsItemHovered();
			const bool held = ImGui::IsItemActive();
			const ImU32 col = held ? button_col_p : (hovered ? button_col_h : button_col_n);
			if (hovered) {
				const ImVec2 pmin = ImGui::GetItemRectMin();
				const ImVec2 pmax = ImGui::GetItemRectMax();
				fg_draw_list->AddRectFilled(
					ImVec2(pmin.x - 3.0f, pmin.y - 3.0f),
					ImVec2(pmax.x + 3.0f, pmax.y + 3.0f),
					IM_COL32(255, 255, 255, 18),
					4.0f
				);
			}
			icon_draw(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col);
			return ImGui::IsItemClicked();
		};

		const float close_x = titlebar_max.x - 18.0f - button_width;
		const float max_x = close_x - 15.0f - button_width;
		const float min_x = max_x - 17.0f - button_width;

		if (draw_button("##titlebar_min", min_x, [&](const ImVec2& bmin, const ImVec2& bmax, ImU32 col) {
			const float cy = (bmin.y + bmax.y) * 0.5f + 2.5f;
			fg_draw_list->AddLine(ImVec2(bmin.x + 1.5f, cy), ImVec2(bmax.x - 1.5f, cy), col, 1.5f);
		})) {
			glfwIconifyWindow(native_window);
		}

		if (draw_button("##titlebar_max", max_x, [&](const ImVec2& bmin, const ImVec2& bmax, ImU32 col) {
			if (is_maximized) {
				fg_draw_list->AddRect(ImVec2(bmin.x + 3.0f, bmin.y + 2.0f), ImVec2(bmax.x - 1.0f, bmax.y - 4.0f), col, 0.0f, 0, 1.2f);
				fg_draw_list->AddRect(ImVec2(bmin.x + 1.0f, bmin.y + 4.0f), ImVec2(bmax.x - 3.0f, bmax.y - 2.0f), col, 0.0f, 0, 1.2f);
			} else {
				fg_draw_list->AddRect(ImVec2(bmin.x + 2.0f, bmin.y + 2.0f), ImVec2(bmax.x - 2.0f, bmax.y - 2.0f), col, 0.0f, 0, 1.2f);
			}
		})) {
			cakery_window->toggle_maximize();
		}

		if (draw_button("##titlebar_close", close_x, [&](const ImVec2& bmin, const ImVec2& bmax, ImU32 col) {
			fg_draw_list->AddLine(ImVec2(bmin.x + 2.0f, bmin.y + 2.0f), ImVec2(bmax.x - 2.0f, bmax.y - 2.0f), col, 1.5f);
			fg_draw_list->AddLine(ImVec2(bmax.x - 2.0f, bmin.y + 2.0f), ImVec2(bmin.x + 2.0f, bmax.y - 2.0f), col, 1.5f);
		})) {
			dodoe::g_context.event_system->publish_event<dodoe::ApplicationQuitEvent>();
		}

		ImGui::End();
		ImGui::PopStyleVar(3);
	}

} // cakery
