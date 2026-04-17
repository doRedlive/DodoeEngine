//
// Created by GreenMuffin on 2026/2/22.
//

#include "project_panel.h"

#include "runtime/core/project/project.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/function/render/framework/texture_manager.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {

    ProjectPanel::ProjectPanel() {
        base_directory_ = FileSystem::asset_path;
        cur_directory_ = base_directory_;

        directory_icon_ = ResourceManager::self().get_texture("engine/res/pictures/ContentBrowser/DirectoryIcon.png",
			"engine/res/pictures/ContentBrowser/DirectoryIcon.png");
        file_icon_ = ResourceManager::self().get_texture("engine/res/pictures/ContentBrowser/FileIcon.png",
			"engine/res/pictures/ContentBrowser/FileIcon.png");

		initializeIconTextures();
    }

	ProjectPanel::~ProjectPanel() {
		cleanup();
	}

	void ProjectPanel::cleanup() {
		directory_icon_texture_ = nullptr;
		file_icon_texture_ = nullptr;
	}

	void ProjectPanel::initializeIconTextures() {
		if (directory_icon_texture_ && file_icon_texture_) {
			return;
		}

		directory_icon_texture_ = TextureManager::self().loadTexture(directory_icon_.id);
		file_icon_texture_ = TextureManager::self().loadTexture(file_icon_.id);
	}

	Ref<Texture> ProjectPanel::getIconTexture(const bool is_directory) const {
		auto texture = is_directory ? directory_icon_texture_ : file_icon_texture_;
		if (texture && texture->handle) {
			return texture;
		}

		return TextureManager::self().loadFallbackTexture();
	}

	void ProjectPanel::draw() {
		ImGui::Begin("Content Browser");

		if (cur_directory_ != fs::path(base_directory_)) {
			if (ImGui::Button("<-")) {
				cur_directory_ = cur_directory_.parent_path();
			}
		}

		static float padding = 16.0f;
		static float thumbnail_size = 128.0f;
		float cellSize = thumbnail_size + padding;

		float panel_width = ImGui::GetContentRegionAvail().x;
		int column_count = (int)(panel_width / cellSize);
		if (column_count < 1)
			column_count = 1;

		ImGui::Columns(column_count, 0, false);

		for (auto& directory_entry : fs::directory_iterator(cur_directory_)) {
			const auto& path = directory_entry.path();
			std::string file_name = path.filename().string();
			const bool is_directory = directory_entry.is_directory();

			ImGui::PushID(file_name.c_str());
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (!directory_icon_texture_ || !file_icon_texture_) {
				initializeIconTextures();
			}

			auto icon_texture = getIconTexture(is_directory);
			if (icon_texture && icon_texture->handle) {
				const ImTextureRef icon_ref(reinterpret_cast<ImTextureID>(icon_texture->handle.Get()));
				ImGui::ImageButton(file_name.c_str(), icon_ref, ImVec2(thumbnail_size, thumbnail_size), ImVec2(0, 1), ImVec2(1, 0));
			} else {
				ImGui::Button(file_name.c_str(), ImVec2(thumbnail_size, thumbnail_size));
			}

			if (ImGui::BeginDragDropSource())
			{
				fs::path relative_path(path);
				const std::string item_path = relative_path.string();
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", item_path.c_str(), item_path.size() + 1);
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (is_directory)
					cur_directory_ /= path.filename();

			}
			ImGui::TextWrapped("%s", file_name.c_str());

			ImGui::NextColumn();

			ImGui::PopID();
		}

		ImGui::Columns(1);

		ImGui::SliderFloat("Thumbnail Size", &thumbnail_size, 16, 512);
		ImGui::SliderFloat("Padding", &padding, 0, 32);

		ImGui::End();
	}

} // cakery
