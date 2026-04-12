// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

	class RhiContext;

	class ImGuiPass : public RenderPass {
		RhiContext* rhi_backend_{nullptr};
		std::vector<rhi::TextureHandle> swapchain_targets_{};
		Vector2i target_extent_{0, 0};
		rhi::CommandListHandle cmd_list_{};
		size_t current_framebuffer_index_{0};

		VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
	public:
		ImGuiPass(const RenderPassCreateInfo& info, RhiContext* rhi_backend, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent);

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;

	private:
		bool initializeVulkanBackend();
	};

} // dodoe
