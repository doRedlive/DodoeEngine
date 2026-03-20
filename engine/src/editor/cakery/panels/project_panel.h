//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef CAKERY_PROJECT_PANEL_H
#define CAKERY_PROJECT_PANEL_H

#include "dopch.h"

#include "runtime/function/render/backend/texture.h"

namespace fs = std::filesystem;

namespace cakery {
	class ProjectPanel {
	public:
		ProjectPanel();
		~ProjectPanel() = default;

		void on_ui_render();

	private:
		fs::path cur_directory_;
		fs::path base_directory_;

		dodoe::Ref<dodoe::Texture> directory_icon_;
		dodoe::Ref<dodoe::Texture> file_icon_;
	};
}

#endif//CAKERY_PROJECT_PANEL_H
