//
// Created by GreenMuffin on 2026/2/22.
//

#include "viewport_panel.h"

#include "runtime/function/render/render_api.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_vulkan.h"

using namespace dodoe;

namespace cakery {
	ViewportPanel::~ViewportPanel() {
		cleanup();
	}

	ViewportPanel::ViewportPanel(dodoe::RhiContext* backend, const std::vector<dodoe::rhi::TextureHandle>& textures) : backend_(backend), viewport_textures_(textures) {
		if (RenderApi::apiType() == RenderApiType::Vulkan && backend_ && backend_->getVulkanBackend()) {
			createViewportSampler();
			createViewportTextureSets();
		}
	}

	void ViewportPanel::setTextures(const std::vector<dodoe::rhi::TextureHandle>& textures) {
		viewport_textures_ = textures;
	}

	void ViewportPanel::setCurrentFramebufferIndex(size_t index) {
		current_framebuffer_index_ = index;
	}

	void ViewportPanel::update() {
		rebuildViewportTextureSetsIfNeeded();
	}

	void ViewportPanel::draw() {
		ImGui::Begin("Viewport");
		rebuildViewportTextureSetsIfNeeded();

		if (RenderApi::apiType() == RenderApiType::Vulkan && !viewport_texture_sets_.empty()) {
			const size_t descriptor_index = current_framebuffer_index_ % viewport_texture_sets_.size();
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			if (viewport_texture_sets_[descriptor_index] != VK_NULL_HANDLE && avail.x > 0.0f && avail.y > 0.0f) {
				const ImTextureRef texture_ref(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(viewport_texture_sets_[descriptor_index])));
				ImGui::Image(
					texture_ref,
					avail,
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f)
				);
			} else {
				ImGui::TextUnformatted("Viewport texture is not ready.");
			}
		} else {
			ImGui::TextUnformatted("No viewport textures.");
		}

		ImGui::End();
	}

	void ViewportPanel::cleanup() {
		for (const auto descriptor_set : viewport_texture_sets_) {
			if (descriptor_set != VK_NULL_HANDLE) {
				ImGui_ImplVulkan_RemoveTexture(descriptor_set);
			}
		}
		viewport_texture_sets_.clear();
		bound_viewport_textures_.clear();
		viewport_textures_.clear();

		if (viewport_sampler_ != VK_NULL_HANDLE) {
			vkDestroySampler(backend_->getVulkanBackend()->getDevice(), viewport_sampler_, nullptr);
			viewport_sampler_ = VK_NULL_HANDLE;
		}
	}

    void ViewportPanel::createViewportSampler() {
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
		if (vkCreateSampler(backend_->getVulkanBackend()->getDevice(), &sampler_info, nullptr, &viewport_sampler_) != VK_SUCCESS) {
			viewport_sampler_ = VK_NULL_HANDLE;
		}
    }

    void ViewportPanel::createViewportTextureSets() {
		viewport_texture_sets_.clear();
		viewport_texture_sets_.reserve(viewport_textures_.size());
		bound_viewport_textures_.clear();
		bound_viewport_textures_.reserve(viewport_textures_.size());
		if (viewport_sampler_ == VK_NULL_HANDLE) {
			createViewportSampler();
		}
		for (const auto& texture : viewport_textures_) {
			bound_viewport_textures_.push_back(texture);
			if (viewport_sampler_ == VK_NULL_HANDLE) {
				viewport_texture_sets_.push_back(VK_NULL_HANDLE);
				continue;
			}
			if (!texture) {
				viewport_texture_sets_.push_back(VK_NULL_HANDLE);
				continue;
			}

			const auto& texture_desc = texture->getDesc();
			auto image_view = static_cast<VkImageView>(texture->getNativeView(
				rhi::ObjectTypes::VK_ImageView,
				texture_desc.format,
				rhi::AllSubresources,
				texture_desc.dimension));
			if (!image_view) {
				viewport_texture_sets_.push_back(VK_NULL_HANDLE);
				continue;
			}

            viewport_texture_sets_.push_back(ImGui_ImplVulkan_AddTexture(
                viewport_sampler_,
                image_view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            ));
        }
    }

	void ViewportPanel::rebuildViewportTextureSetsIfNeeded() {
		bool need_rebuild = (viewport_texture_sets_.size() != viewport_textures_.size());
		if (!need_rebuild && bound_viewport_textures_.size() == viewport_textures_.size()) {
			for (size_t i = 0; i < viewport_textures_.size(); ++i) {
				if (bound_viewport_textures_[i] != viewport_textures_[i]) {
					need_rebuild = true;
					break;
				}
			}
		}
		if (!need_rebuild) {
			for (const auto descriptor_set : viewport_texture_sets_) {
				if (descriptor_set == VK_NULL_HANDLE) {
					need_rebuild = true;
					break;
				}
			}
		}

		if (!need_rebuild) {
			return;
		}

		for (const auto descriptor_set : viewport_texture_sets_) {
			if (descriptor_set != VK_NULL_HANDLE) {
				ImGui_ImplVulkan_RemoveTexture(descriptor_set);
			}
		}
		createViewportTextureSets();
	}

} // cakery