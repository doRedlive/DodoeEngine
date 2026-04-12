// Create by Redlive on 2026/4/8.

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "../render_resource.h"

namespace dodoe {
	class Camera;

	class SpritePass : public RenderPass {
		Vector2i target_extent_{0, 0};
		size_t target_count_{0};
		Camera* camera_{nullptr};
		std::vector<identifier> bound_texture_ids_{};

		std::vector<rhi::TextureHandle> scene_targets_{};
		std::vector<rhi::FramebufferHandle> framebuffers_{};
		rhi::BufferHandle vertex_buffer_{};
		rhi::BufferHandle index_buffer_{};
		rhi::BufferHandle camera_buffer_{};

		rhi::GraphicsPipelineHandle graphics_pipeline_{};
		rhi::BindingLayoutHandle binding_layout_{};
		rhi::BindingSetHandle binding_set_{};
		rhi::SamplerHandle sampler_{};
	public:
		SpritePass(const RenderPassCreateInfo& info, size_t target_count, const Vector2i& target_extent, Camera* camera);
		[[nodiscard]] const std::vector<rhi::TextureHandle>& scene_targets() const { return scene_targets_; }

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;

	private:
		void createBuffers();
		void createSampler();
		void createBindingLayout();
		void createBindingSet(const std::vector<identifier>& texture_ids);
		void createFramebuffers();
		void createGraphicsPipeline();

		bool checkBindingSet(const std::vector<identifier>& cpu_texture_ids);
	};

} // dodoe
