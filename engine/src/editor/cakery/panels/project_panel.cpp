//
// Created by GreenMuffin on 2026/2/22.
//

#include "project_panel.h"

#include "runtime/core/project/project.h"
#include "runtime/resource/resource_manager.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {

    ProjectPanel::ProjectPanel() {
        base_directory_ = FileSystem::asset_path;
        cur_directory_ = base_directory_;

        directory_icon_ = ResourceManager::self()
                              .get_texture("pictures/ContentBrowser/DirectoryIcon.png",
                                           "pictures/ContentBrowser/DirectoryIcon.png")
                              .texture;
        file_icon_ = ResourceManager::self()
                         .get_texture("pictures/ContentBrowser/FileIcon.png",
                                      "pictures/ContentBrowser/FileIcon.png")
                         .texture;
    }

	void ProjectPanel::on_ui_render() {
		ImGui::Begin("Content Browser");

		if (cur_directory_ != fs::path(base_directory_))
		{
			if (ImGui::Button("<-"))
			{
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

		for (auto& directory_entry : fs::directory_iterator(cur_directory_))
		{
			const auto& path = directory_entry.path();
			std::string file_name = path.filename().string();

			ImGui::PushID(file_name.c_str());
			Ref<Texture> icon = directory_entry.is_directory() ? directory_icon_ : file_icon_;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::ImageButton(file_name.c_str(), static_cast<ImTextureID>(static_cast<uintptr_t>(icon ? icon->id : 0)), { thumbnail_size, thumbnail_size }, { 0, 1 }, { 1, 0 });

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
				if (directory_entry.is_directory())
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
