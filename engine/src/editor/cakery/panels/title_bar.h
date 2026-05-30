// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

namespace dodoe {
	class Window;
	struct Texture;
}

namespace cakery {

	class EditorPanelManager;

	class Titlebar : public EditorPanel {
	public:
		explicit Titlebar(EditorPanelDescriptor descriptor);
		void onDraw(const EditorPanelContext& context) override;

	private:
		void ensureButtonTextures();
		void drawWorldStateButtons();

		bool show_settings_{false};
		bool show_imgui_demo_{false};
		bool lock_viewport_{true};
		float ui_scale_{1.0f};
		dodoe::Ref<dodoe::Texture> play_button_texture_{nullptr};
		dodoe::Ref<dodoe::Texture> stop_button_texture_{nullptr};
	};

} // cakery
