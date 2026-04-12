//
// Created by GreenMuffin on 2026/2/22.
//

#include "project_panel.h"

#include "runtime/core/project/project.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/function/render/render_api.h"
#include "runtime/function/render/render_helper.h"
#include "runtime/function/render/render_system.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"

using namespace dodoe;

namespace cakery {

    ProjectPanel::ProjectPanel() {
        base_directory_ = FileSystem::asset_path;
        cur_directory_ = base_directory_;

        directory_icon_ = ResourceManager::self().get_texture("engine/res/pictures/ContentBrowser/DirectoryIcon.png",
			"engine/res/pictures/ContentBrowser/DirectoryIcon.png");
        file_icon_ = ResourceManager::self().get_texture("engine/res/pictures/ContentBrowser/FileIcon.png",
			"engine/res/pictures/ContentBrowser/FileIcon.png");

		initializeVulkanIconDescriptors();
    }

	ProjectPanel::~ProjectPanel() {
		cleanup();
	}

	void ProjectPanel::cleanup() {
		if (RenderApi::apiType() != RenderApiType::Vulkan) {
			return;
		}

		if (directory_icon_set_ != VK_NULL_HANDLE) {
			ImGui_ImplVulkan_RemoveTexture(directory_icon_set_);
			directory_icon_set_ = VK_NULL_HANDLE;
		}
		if (file_icon_set_ != VK_NULL_HANDLE) {
			ImGui_ImplVulkan_RemoveTexture(file_icon_set_);
			file_icon_set_ = VK_NULL_HANDLE;
		}

		if (icon_sampler_ != VK_NULL_HANDLE && icon_device_ != VK_NULL_HANDLE) {
			vkDestroySampler(icon_device_, icon_sampler_, nullptr);
			icon_sampler_ = VK_NULL_HANDLE;
		}
		icon_device_ = VK_NULL_HANDLE;
	}

	void ProjectPanel::initializeVulkanIconDescriptors() {
		if (RenderApi::apiType() != RenderApiType::Vulkan) {
			return;
		}
		if (directory_icon_set_ != VK_NULL_HANDLE && file_icon_set_ != VK_NULL_HANDLE) {
			return;
		}

		auto* render_system = dodoe::Application::self().context().render_system.get();
		auto* rhi_backend = render_system ? render_system->rhiBackend() : nullptr;
		auto* vulkan_backend = rhi_backend ? rhi_backend->getVulkanBackend() : nullptr;
		if (!vulkan_backend) {
			return;
		}
		icon_device_ = vulkan_backend->getDevice();
		if (icon_device_ == VK_NULL_HANDLE) {
			return;
		}

		if (icon_sampler_ == VK_NULL_HANDLE) {
			VkSamplerCreateInfo sampler_info{};
			sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_info.magFilter = VK_FILTER_LINEAR;
			sampler_info.minFilter = VK_FILTER_LINEAR;
			sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = 1.0f;
			if (vkCreateSampler(icon_device_, &sampler_info, nullptr, &icon_sampler_) != VK_SUCCESS) {
				icon_device_ = VK_NULL_HANDLE;
				return;
			}
		}
		if (icon_sampler_ == VK_NULL_HANDLE) {
			return;
		}

		if (directory_icon_set_ != VK_NULL_HANDLE) {
			ImGui_ImplVulkan_RemoveTexture(directory_icon_set_);
			directory_icon_set_ = VK_NULL_HANDLE;
		}
		if (file_icon_set_ != VK_NULL_HANDLE) {
			ImGui_ImplVulkan_RemoveTexture(file_icon_set_);
			file_icon_set_ = VK_NULL_HANDLE;
		}

		auto create_icon_set = [this](const TextureRes& icon_res) -> VkDescriptorSet {
			if (icon_sampler_ == VK_NULL_HANDLE) {
				return VK_NULL_HANDLE;
			}
			auto texture = TextureManager::self().getTexture(icon_res);
			if (!texture) {
				return VK_NULL_HANDLE;
			}
			auto image_view = static_cast<VkImageView>(texture->getNativeView(rhi::ObjectTypes::VK_ImageView));
			if (!image_view) {
				return VK_NULL_HANDLE;
			}
			return ImGui_ImplVulkan_AddTexture(icon_sampler_, image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		};

		directory_icon_set_ = create_icon_set(directory_icon_);
		file_icon_set_ = create_icon_set(file_icon_);
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

			ImGui::PushID(file_name.c_str());
			TextureRes icon = directory_entry.is_directory() ? directory_icon_ : file_icon_;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (RenderApi::apiType() == RenderApiType::Vulkan) {
				if (directory_icon_set_ == VK_NULL_HANDLE || file_icon_set_ == VK_NULL_HANDLE) {
					initializeVulkanIconDescriptors();
				}
				const VkDescriptorSet icon_set = directory_entry.is_directory() ? directory_icon_set_ : file_icon_set_;
				if (icon_set != VK_NULL_HANDLE) {
					const ImTextureRef icon_ref(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(icon_set)));
					ImGui::ImageButton(file_name.c_str(), icon_ref, ImVec2(thumbnail_size, thumbnail_size), ImVec2(0, 1), ImVec2(1, 0));
				} else {
					ImGui::Button(file_name.c_str(), ImVec2(thumbnail_size, thumbnail_size));
				}
			} else {
				const ImTextureRef icon_ref(static_cast<ImTextureID>(static_cast<uintptr_t>(icon.id)));
				ImGui::ImageButton(file_name.c_str(), icon_ref, ImVec2(thumbnail_size, thumbnail_size), ImVec2(0, 1), ImVec2(1, 0));
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
