//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef CAKERY_PROJECT_PANEL_H
#define CAKERY_PROJECT_PANEL_H

#include "dopch.h"

#include "runtime/resource/resource_type.h"
#include "runtime/function/render/interface/vulkan_backend.h"

namespace fs = std::filesystem;

namespace cakery {
	class ProjectPanel {
		fs::path cur_directory_;
		fs::path base_directory_;
		dodoe::TextureRes directory_icon_;
		dodoe::TextureRes file_icon_;
		VkSampler icon_sampler_{VK_NULL_HANDLE};
		VkDevice icon_device_{VK_NULL_HANDLE};
		VkDescriptorSet directory_icon_set_{VK_NULL_HANDLE};
		VkDescriptorSet file_icon_set_{VK_NULL_HANDLE};
	public:
		ProjectPanel();
		~ProjectPanel();
		void draw();
		void cleanup();
	private:
		void initializeVulkanIconDescriptors();
	};
}

#endif//CAKERY_PROJECT_PANEL_H
