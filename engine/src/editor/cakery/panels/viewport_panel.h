//
// Created by GreenMuffin on 2026/2/22.
//

#pragma once

#include "dopch.h"

#include "runtime/function/render/interface/rhi.h"
#include "runtime/function/render/interface/rhi_backend.h"

namespace cakery {
	class ViewportPanel {
        dodoe::RhiContext* backend_{nullptr};
        VkSampler viewport_sampler_{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> viewport_texture_sets_{}; 
        std::vector<dodoe::rhi::TextureHandle> viewport_textures_{};
		std::vector<dodoe::rhi::TextureHandle> bound_viewport_textures_{};
        size_t current_framebuffer_index_{0};
	public:
        ~ViewportPanel();
        ViewportPanel(dodoe::RhiContext* backend, const std::vector<dodoe::rhi::TextureHandle>& textures);

        void setTextures(const std::vector<dodoe::rhi::TextureHandle>& textures);
        void setCurrentFramebufferIndex(size_t index);
		void update();
		void draw();
        void cleanup();
    private:
        void createViewportSampler();
        void createViewportTextureSets();
        void rebuildViewportTextureSetsIfNeeded();
	};

} // cakery
