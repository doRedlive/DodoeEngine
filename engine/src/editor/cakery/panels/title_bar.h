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
	private:
		Bool m_show_settings{false};
		Bool m_show_imgui_demo{false};
		Bool m_lock_viewport{true};
		float m_ui_scale{1.0f};
		dodoe::Ref<dodoe::Texture> m_play_button_texture{nullptr};
		dodoe::Ref<dodoe::Texture> m_pause_button_texture{nullptr};
		dodoe::Ref<dodoe::Texture> m_stop_button_texture{nullptr};

	public:
		explicit Titlebar(EditorPanelDescriptor descriptor);
		void onDraw(const EditorPanelContext& context) override;

	private:
		void ensureButtonTextures();
		void drawWorldStateButtons();
	};

} // cakery
