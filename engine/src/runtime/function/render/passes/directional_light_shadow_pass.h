// Created by Redlive on 2026/5/4.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../render_resource.h"

namespace dodoe {
	class DirectionalLightShadowPass : public RenderPass {
		inline static const std::string kSceneShadowMapName = "ShadowMap";

		struct DirectionalLightShadowPassConstants {
			Matrix4f light_view_projection{1.0f};
		};

		rhi::BufferHandle m_constant_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::InputLayoutHandle m_input_layout{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};
		rhi::TextureHandle m_shadow_target{};
		rhi::FramebufferHandle m_framebuffer{};

	public:
		explicit DirectionalLightShadowPass(RhiContext* rhi);
		~DirectionalLightShadowPass() override = default;

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onViewportResize(const Vector2i& viewport_extent) override;

	private:
		void createBuffers();
		void createShaders();
		void createInputLayout();
		void createBindingLayout();
		void createBindingSet();
		void createFramebuffer();
		void createGraphicsPipeline();
	};

} // dodoe
