//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef CAKERY_PROJECT_PANEL_H
#define CAKERY_PROJECT_PANEL_H

#include "dopch.h"

#include "runtime/resource/resource_type.h"
#include "runtime/function/render/framework/texture_manager.h"

namespace fs = std::filesystem;

namespace cakery {
	class ProjectPanel {
		fs::path cur_directory_;
		fs::path base_directory_;
		dodoe::TextureRes directory_icon_;
		dodoe::TextureRes file_icon_;
		dodoe::Ref<dodoe::Texture> directory_icon_texture_{nullptr};
		dodoe::Ref<dodoe::Texture> file_icon_texture_{nullptr};
	public:
		ProjectPanel();
		~ProjectPanel();
		void draw();
		void cleanup();
	private:
		void initializeIconTextures();
		[[nodiscard]] dodoe::Ref<dodoe::Texture> getIconTexture(bool is_directory) const;
	};
}

#endif//CAKERY_PROJECT_PANEL_H
