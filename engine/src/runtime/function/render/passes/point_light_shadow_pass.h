// Created by Redlive on 2026/5/6.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"

namespace dodoe {
	class PointLightShadowPass : public RenderPass {
		static constexpr ui32 kMaxPointLightCount = 32;
		static constexpr ui32 kShadowMapSize = 1024;
		static constexpr ui32 kShadowLayerCount = kMaxPointLightCount * 2;
		static constexpr ui32 kMaxShadowGeomVertices = kMaxPointLightCount * 6;

		struct PointLightShadowPassConstants {
			ui32 point_light_count{0};
			ui32 padding0{0};
			ui32 padding1{0};
			ui32 padding2{0};
			Vector4f point_lights_position_and_radius[kMaxPointLightCount]{};
		};

		rhi::BufferHandle m_constant_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_geometry_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::InputLayoutHandle m_input_layout{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};
		rhi::TextureHandle m_shadow_target{};
		rhi::FramebufferHandle m_framebuffer{};
		ui32 m_active_layer_count{0};

	public:
		explicit PointLightShadowPass(RhiContext* rhi);
		~PointLightShadowPass() override = default;

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
		void createShadowTarget(ui32 layer_count);
		void createFramebuffer();
		void createGraphicsPipeline();
	};

} // dodoe