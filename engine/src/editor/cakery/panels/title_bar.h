////
//// Created by GreenMuffin on 2026/3/4.
////

#ifndef CAKERY_TITLE_BAR_H
#define CAKERY_TITLE_BAR_H

#include "dopch.h"

namespace dodoe {
	class Window;
	struct Texture;
}

namespace cakery {

	class Titlebar {
	public:
		void draw(dodoe::Window* cakery_window);

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

#endif // CAKERY_TITLE_BAR_H
