// Created by Redlive on 2026/5/7.

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {
	class SkyboxPass : public RenderPass {
		inline static const std::string kSceneHdrColorName = "MainCameraHdrColor";
		inline static const std::string kSceneDepthName = "MainCameraDepth";

		struct SkyboxPushConstants {
			Matrix4f inv_view_projection{1.0f};
		};

		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::SamplerHandle m_sampler{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};
		rhi::FramebufferHandle m_framebuffer{};
		rhi::TextureHandle m_hdr_target{};
		rhi::TextureHandle m_depth_target{};
		rhi::TextureHandle m_skybox_texture{};

	public:
		explicit SkyboxPass(RhiContext* rhi) { m_rhi = rhi; }
		~SkyboxPass() override = default;

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
