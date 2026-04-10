// Create by Redlive on 2026/4/8.

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "../render_resource.h"

namespace dodoe {

	class SpritePass : public RenderPass {
		std::vector<rhi::TextureHandle> swapchain_targets_{};
		Vector2i target_extent_{0, 0};
		std::vector<rhi::FramebufferHandle> framebuffers_{};
		size_t current_framebuffer_index_{0};
		rhi::BufferHandle vertex_buffer_{};
		rhi::BufferHandle index_buffer_{};
		std::vector<identifier> bound_texture_ids_{};

		rhi::GraphicsPipelineHandle graphics_pipeline_{};
		rhi::BindingLayoutHandle binding_layout_{};
		rhi::BindingSetHandle binding_set_{};
		rhi::SamplerHandle sampler_{};
		rhi::CommandListHandle cmd_list_{};
		bool initialized_{false};

	public:
		SpritePass(const RenderPassCreateInfo& info, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent);

		void setup() override;
		void execute() override;
		void cleanup() override;

	private:
		void createBuffers();
		void createSampler();
		void createBindingLayout();
		void createBindingSet(const std::vector<identifier>& texture_ids);
		void createFramebuffers();
		void createGraphicsPipeline();
	};

} // dodoe
