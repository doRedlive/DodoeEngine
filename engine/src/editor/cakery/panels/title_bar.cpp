////
//// Created by GreenMuffin on 2026/3/4.
////

#include "title_bar.h"

#include "cakery/framework/editor_panel_manager.h"

#include "runtime/core/application.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/world/world.h"

#include "imgui/imgui.h"

namespace cakery {
	namespace {
		constexpr const char* kPlayButtonPath = "engine/res/pictures/Buttons/PlayButton.png";
		constexpr const char* kStopButtonPath = "engine/res/pictures/Buttons/StopButton.png";
	}

	Titlebar::Titlebar(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	void Titlebar::ensureButtonTextures() {
		auto& app = dodoe::Application::Self();
		auto* render_system = app.context().render_system.get();
		auto* texture_manager = render_system ? render_system->getTextureManager() : nullptr;
		if (!texture_manager) {
			return;
		}

		if (!play_button_texture_ || !play_button_texture_->handle) {
			play_button_texture_ = texture_manager->loadTexture(kPlayButtonPath);
		}
		if (!stop_button_texture_ || !stop_button_texture_->handle) {
			stop_button_texture_ = texture_manager->loadTexture(kStopButtonPath);
		}
	}

	void Titlebar::drawWorldStateButtons() {
		auto& app = dodoe::Application::Self();
		auto* world = app.context().world.get();
		if (!world) {
			return;
		}

		ensureButtonTextures();

		const bool is_runtime = world->getState() == dodoe::WorldState::Runtime;
		constexpr ImVec2 kButtonSize(20.0f, 20.0f);

		auto draw_button = [&](const char* id, const dodoe::Ref<dodoe::Texture>& texture, const bool enabled) -> bool {
			if (!texture || !texture->handle) {
				return false;
			}

			if (!enabled) {
				ImGui::BeginDisabled();
			}
			const bool clicked = ImGui::ImageButton(
				id,
				ImTextureRef(reinterpret_cast<ImTextureID>(texture->handle.Get())),
				kButtonSize,
				ImVec2(0, 1),
				ImVec2(1, 0));
			if (!enabled) {
				ImGui::EndDisabled();
			}
			return enabled && clicked;
		};

		if (draw_button("##PlayButton", play_button_texture_, !is_runtime)) {
			world->finalize();
			world->setState(dodoe::WorldState::Runtime);
			world->start();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Start Runtime");
		}

		ImGui::SameLine();

		if (draw_button("##StopButton", stop_button_texture_, is_runtime)) {
			world->finalize();
			world->setState(dodoe::WorldState::Simulation);
			world->start();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Stop Runtime");
		}
	}

    void Titlebar::onDraw(const EditorPanelContext& context) {
		if (!ImGui::BeginMainMenuBar()) {
			return;
		}

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				dodoe::EventSystem::Publish<dodoe::ApplicationQuitEvent>();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (context.panel_manager) {
				context.panel_manager->drawViewMenuItems();
			}
			ImGui::Separator();
			ImGui::MenuItem("ImGui Demo", nullptr, &show_imgui_demo_);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings")) {
			if (ImGui::MenuItem("Editor Settings")) {
				show_settings_ = true;
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();
		drawWorldStateButtons();
		ImGui::Separator();
		ImGui::TextUnformatted(dodoe::Application::Self().specification().name.c_str());
		ImGui::EndMainMenuBar();

		if (show_settings_) {
			if (ImGui::Begin("Settings", &show_settings_)) {
				ImGui::TextUnformatted("Editor");
				ImGui::Separator();
				ImGui::Checkbox("Lock Viewport Camera", &lock_viewport_);
				ImGui::SliderFloat("UI Scale", &ui_scale_, 0.75f, 1.5f, "%.2f");
				ImGui::TextDisabled("Note: this panel is ready for wiring project/runtime settings.");
			}
			ImGui::End();
		}

		if (show_imgui_demo_) {
			ImGui::ShowDemoWindow(&show_imgui_demo_);
		}
    }

} // cakery
