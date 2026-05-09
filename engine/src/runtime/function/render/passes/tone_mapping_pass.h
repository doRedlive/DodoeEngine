// Created by Redlive on 2026/5/7.

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {
	class ToneMappingPass : public RenderPass {
		inline static const std::string kInputColorResourceName = "MainCameraHdrColor";
		inline static const std::string kOutputColorResourceName = "MainCameraToneMappedColor";

		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::SamplerHandle m_sampler{};
		rhi::TextureHandle m_input_color_target{};
		rhi::TextureHandle m_output_color_target{};
		rhi::FramebufferHandle m_framebuffer{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};

	public:
		explicit ToneMappingPass(RhiContext* rhi) { m_rhi = rhi; }
		~ToneMappingPass() override = default;

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onViewportResize(const Vector2i& viewport_extent) override;

	private:
		void refreshInputResources();
		void createShaders();
		void createSampler();
		void createBindingLayout();
		void createBindingSet();
		void createFramebuffer();
		void createGraphicsPipeline();
	};

} // dodoe