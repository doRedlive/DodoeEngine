#pragma once

#include <imgui.h>
#include "GLFW/glfw3.h"

#include <algorithm>

namespace dodoe {

	inline float ResolveImGuiScale(GLFWwindow* window) {
		float x_scale = 1.0f;
		float y_scale = 1.0f;
		if (window != nullptr) {
			glfwGetWindowContentScale(window, &x_scale, &y_scale);
		}

		const float content_scale = (std::max)((std::max)(x_scale, y_scale), 1.0f);
		return std::clamp(content_scale, 1.0f, 1.35f);
	}

	inline void ApplyImGuiStyle(GLFWwindow* window) {
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigDpiScaleFonts = false;
		io.ConfigDpiScaleViewports = false;

		const float ui_scale = ResolveImGuiScale(window);

		// Larger font for readability
		ImFontConfig font_config;
		font_config.SizePixels = 17.0f * ui_scale;
		font_config.OversampleH = 3;
		font_config.OversampleV = 2;
		font_config.RasterizerMultiply = 1.3f;
		font_config.PixelSnapH = false;
		io.Fonts->Clear();
		io.FontDefault = io.Fonts->AddFontDefaultVector(&font_config);

		ImGuiStyle style;
		ImGui::StyleColorsDark(&style);

		// ---- Island Style: rounded floating panels ----
		style.WindowRounding = 10.0f;
		style.ChildRounding = 6.0f;
		style.PopupRounding = 6.0f;
		style.FrameRounding = 6.0f;
		style.ScrollbarRounding = 6.0f;
		style.GrabRounding = 6.0f;
		style.TabRounding = 8.0f;
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(6.0f, 5.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 20.0f;
		style.ScrollbarSize = 10.0f;
		style.GrabMinSize = 10.0f;
		style.SeparatorTextPadding = ImVec2(8.0f, 4.0f);
		// Island gap: space between docked panels
		style.DockingSeparatorSize = 4.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.ScaleAllSizes(ui_scale);

		ImVec4* colors = style.Colors;
		auto rgba255 = [](int r, int g, int b, int a) {
			return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
		};

		// ---- Original Purple Theme (restored) ----
		colors[ImGuiCol_Text] = rgba255(230, 230, 235, 255);
		colors[ImGuiCol_TextDisabled] = rgba255(148, 148, 153, 255);
		colors[ImGuiCol_WindowBg] = rgba255(33, 34, 44, 255);
		colors[ImGuiCol_ChildBg] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_PopupBg] = rgba255(20, 20, 20, 240);
		colors[ImGuiCol_Border] = rgba255(152, 108, 213, 70);
		colors[ImGuiCol_BorderShadow] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_FrameBg] = rgba255(51, 61, 77, 255);
		colors[ImGuiCol_FrameBgHovered] = rgba255(66, 79, 97, 255);
		colors[ImGuiCol_FrameBgActive] = rgba255(79, 97, 120, 255);
		colors[ImGuiCol_TitleBg] = rgba255(25, 26, 33, 255);
		colors[ImGuiCol_TitleBgActive] = rgba255(25, 26, 33, 255);
		colors[ImGuiCol_TitleBgCollapsed] = rgba255(0, 0, 0, 128);
		colors[ImGuiCol_MenuBarBg] = rgba255(20, 20, 28, 255);
		colors[ImGuiCol_ScrollbarBg] = rgba255(5, 5, 5, 135);
		colors[ImGuiCol_ScrollbarGrab] = rgba255(79, 79, 79, 255);
		colors[ImGuiCol_ScrollbarGrabHovered] = rgba255(105, 105, 105, 255);
		colors[ImGuiCol_ScrollbarGrabActive] = rgba255(130, 130, 130, 255);
		colors[ImGuiCol_CheckMark] = rgba255(255, 121, 198, 255);
		colors[ImGuiCol_SliderGrab] = rgba255(255, 121, 198, 255);
		colors[ImGuiCol_SliderGrabActive] = rgba255(152, 108, 213, 230);
		colors[ImGuiCol_Button] = rgba255(3, 3, 3, 255);
		colors[ImGuiCol_ButtonHovered] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_ButtonActive] = rgba255(189, 147, 249, 255);
		colors[ImGuiCol_Header] = rgba255(152, 108, 213, 194);
		colors[ImGuiCol_HeaderHovered] = rgba255(152, 108, 213, 235);
		colors[ImGuiCol_HeaderActive] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_Separator] = rgba255(152, 108, 213, 209);
		colors[ImGuiCol_SeparatorHovered] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_SeparatorActive] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_ResizeGrip] = rgba255(66, 150, 250, 51);
		colors[ImGuiCol_ResizeGripHovered] = rgba255(66, 150, 250, 171);
		colors[ImGuiCol_ResizeGripActive] = rgba255(66, 150, 250, 242);
		colors[ImGuiCol_InputTextCursor] = rgba255(255, 255, 255, 255);
		colors[ImGuiCol_TabHovered] = rgba255(152, 108, 213, 190);
		colors[ImGuiCol_Tab] = rgba255(6, 6, 8, 255);
		colors[ImGuiCol_TabSelected] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_TabSelectedOverline] = rgba255(66, 150, 250, 255);
		colors[ImGuiCol_TabDimmed] = rgba255(152, 108, 213, 40);
		colors[ImGuiCol_TabDimmedSelected] = rgba255(6, 6, 8, 200);
		colors[ImGuiCol_TabDimmedSelectedOverline] = rgba255(128, 128, 128, 0);
		colors[ImGuiCol_DockingPreview] = rgba255(152, 108, 213, 209);
		colors[ImGuiCol_DockingEmptyBg] = rgba255(18, 18, 22, 255);
		colors[ImGuiCol_PlotLines] = rgba255(156, 156, 156, 255);
		colors[ImGuiCol_PlotLinesHovered] = rgba255(255, 110, 89, 255);
		colors[ImGuiCol_PlotHistogram] = rgba255(230, 179, 0, 255);
		colors[ImGuiCol_PlotHistogramHovered] = rgba255(255, 153, 0, 255);
		colors[ImGuiCol_TableHeaderBg] = rgba255(0, 0, 0, 255);
		colors[ImGuiCol_TableBorderStrong] = rgba255(152, 108, 213, 255);
		colors[ImGuiCol_TableBorderLight] = rgba255(152, 108, 213, 153);
		colors[ImGuiCol_TableRowBg] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_TableRowBgAlt] = rgba255(255, 255, 255, 15);
		colors[ImGuiCol_TextLink] = rgba255(66, 150, 250, 255);
		colors[ImGuiCol_TextSelectedBg] = rgba255(66, 150, 250, 89);
		colors[ImGuiCol_TreeLines] = rgba255(110, 110, 128, 128);
		colors[ImGuiCol_DragDropTarget] = rgba255(255, 255, 0, 230);
		colors[ImGuiCol_DragDropTargetBg] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_UnsavedMarker] = rgba255(255, 255, 255, 255);
		colors[ImGuiCol_NavCursor] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_NavWindowingHighlight] = rgba255(0, 0, 0, 0);
		colors[ImGuiCol_NavWindowingDimBg] = rgba255(204, 204, 204, 51);
		colors[ImGuiCol_ModalWindowDimBg] = rgba255(204, 204, 204, 89);

		ImGui::GetStyle() = style;
	}

} // dodoe
