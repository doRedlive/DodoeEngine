////
//// Created by GreenMuffin on 2026/3/4.
////

#include "title_bar.h"

#include "cakery/framework/editor_panel_manager.h"

#include "runtime/function/render/render_system.h"
#include "runtime/core/application.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/world/world.h"

#include "imgui/imgui.h"

namespace cakery {
	namespace {
		constexpr const char* kPlayButtonPath = "engine/res/pictures/Buttons/PlayButton.png";
		constexpr const char* kStopButtonPath = "engine/res/pictures/Buttons/StopButton.png";
		constexpr const char* kPauseButtonPath = "engine/res/pictures/Buttons/PauseButton.png";
		constexpr float kToolbarHeight = 48.0f;
		constexpr float kToolbarMargin = 6.0f;
		constexpr float kToolbarSideMargin = 8.0f;
	}

	Titlebar::Titlebar(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	void Titlebar::ensureButtonTextures() {
		if (!m_play_button_texture || !m_play_button_texture->getGpuHandle()) {
			m_play_button_texture = dodoe::Texture::Load(kPlayButtonPath);
		}
		if (!m_stop_button_texture || !m_stop_button_texture->getGpuHandle()) {
			m_stop_button_texture = dodoe::Texture::Load(kStopButtonPath);
		}
		if (!m_pause_button_texture || !m_pause_button_texture->getGpuHandle()) {
			m_pause_button_texture = dodoe::Texture::Load(kPauseButtonPath);
		}
	}

	void Titlebar::drawWorldStateButtons() {
		auto& app = dodoe::Application::Self();
		auto* world = app.context().world.get();
		if (!world) {
			return;
		}

		ensureButtonTextures();

		const auto state = world->getState();
		const Bool is_simulation = state == dodoe::WorldState::Simulation;
		const Bool is_runtime = state == dodoe::WorldState::Runtime;
		const Bool is_pause = state == dodoe::WorldState::Pause;
		constexpr float kButtonSizePx = 32.0f;
		constexpr ImVec2 kButtonSize(kButtonSizePx, kButtonSizePx);
		constexpr float kButtonGap = 10.0f;

		auto draw_button = [&](const char* id, const dodoe::Ref<dodoe::Texture>& texture, const Bool enabled, const Bool highlight) -> Bool {
			if (!texture || !texture->getGpuHandle()) {
				return false;
			}

			(void)highlight;
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

			if (!enabled) {
				ImGui::BeginDisabled();
			}
			const Bool clicked = ImGui::ImageButton(
				id,
				ImTextureRef(reinterpret_cast<ImTextureID>(texture->getGpuHandle().Get())),
				kButtonSize,
				ImVec2(0, 1),
				ImVec2(1, 0));
			if (!enabled) {
				ImGui::EndDisabled();
			}

			ImGui::PopStyleVar();
			return enabled && clicked;
		};

		if (draw_button("##PlayButton", m_play_button_texture, !is_runtime, is_runtime)) {
			world->finalize();
			world->setState(dodoe::WorldState::Runtime);
			world->start();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Play");
		}

		ImGui::SameLine(0.0f, kButtonGap);

		// Pause button (active when in runtime)
		if (draw_button("##PauseButton", m_pause_button_texture, is_runtime || is_pause, is_pause)) {
			if (is_pause) {
				// Unpause: go back to runtime
				world->setState(dodoe::WorldState::Runtime);
			} else {
				// Pause: runtime -> pause
				world->setState(dodoe::WorldState::Pause);
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(is_pause ? "Resume" : "Pause");
		}

		ImGui::SameLine(0.0f, kButtonGap);

		// Stop button (active when in runtime or pause)
		if (draw_button("##StopButton", m_stop_button_texture, !is_simulation, false)) {
			world->finalize();
			world->setState(dodoe::WorldState::Simulation);
			world->start();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Stop");
		}
	}

	void Titlebar::onDraw(const EditorPanelContext& context) {
		// ---- Menu Bar ----
		if (!ImGui::BeginMainMenuBar()) {
			return;
		}

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				dodoe::EventSystem::Publish<dodoe::ApplicationQuitEvent>();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			ImGui::MenuItem("Undo", nullptr, false, false);
			ImGui::MenuItem("Redo", nullptr, false, false);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (context.panel_manager) {
				context.panel_manager->drawViewMenuItems();
			}
			ImGui::Separator();
			ImGui::MenuItem("ImGui Demo", nullptr, &m_show_imgui_demo);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings")) {
			if (ImGui::MenuItem("Editor Settings")) {
				m_show_settings = true;
			}
			ImGui::EndMenu();
		}

		// Project name on the right
		{
			const String& project_name = dodoe::Application::Self().specification().name;
			if (!project_name.empty()) {
				const float text_width = ImGui::CalcTextSize(project_name.c_str()).x;
				const float window_width = ImGui::GetWindowWidth();
				ImGui::SameLine(window_width - text_width - ImGui::GetStyle().WindowPadding.x * 2.0f);
				ImGui::TextDisabled("%s", project_name.c_str());
			}
		}

		ImGui::EndMainMenuBar();

		// ---- Toolbar: aligned with panels, centered buttons ----
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			const float toolbar_x = viewport->WorkPos.x + kToolbarSideMargin;
			const float toolbar_w = viewport->WorkSize.x - kToolbarSideMargin * 2.0f;
			const ImVec2 toolbar_pos(toolbar_x, viewport->WorkPos.y + kToolbarMargin);
			const ImVec2 toolbar_size(toolbar_w, kToolbarHeight);

			ImGui::SetNextWindowPos(toolbar_pos);
			ImGui::SetNextWindowSize(toolbar_size);

			ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
				| ImGuiWindowFlags_NoFocusOnAppearing;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.129f, 0.133f, 0.172f, 1.0f));

			ImGui::Begin("##Toolbar", nullptr, flags);

			// Center the play controls in the toolbar
			constexpr float kBtnSize = 32.0f;
			constexpr float kGap = 10.0f;
			const float btns_total_w = kBtnSize * 3.0f + kGap * 2.0f;
			ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btns_total_w) * 0.5f);
			drawWorldStateButtons();

			ImGui::End();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(3);
		}

		if (m_show_settings) {
			if (ImGui::Begin("Settings", &m_show_settings)) {
				ImGui::TextUnformatted("Editor");
				ImGui::Separator();
				ImGui::Checkbox("Lock Viewport Camera", &m_lock_viewport);
				ImGui::SliderFloat("UI Scale", &m_ui_scale, 0.75f, 1.5f, "%.2f");
			}
			ImGui::End();
		}

		if (m_show_imgui_demo) {
			ImGui::ShowDemoWindow(&m_show_imgui_demo);
		}
	}

} // cakery
