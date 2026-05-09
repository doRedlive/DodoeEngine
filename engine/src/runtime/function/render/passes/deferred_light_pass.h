// Created by Redlive on 2026/5/4.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../render_resource.h"

namespace dodoe {
	struct DeferredLightPassConstants {
		Vector4f light_color_intensity{1.0f, 1.0f, 1.0f, 1.0f};
		Vector4f light_position_radius{0.0f, 0.0f, 0.0f, 0.0f};
		Vector4f light_direction_type{0.0f, 0.0f, 0.0f, 0.0f};
		Matrix4f light_view_projection{1.0f};
		Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
		Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
	};

	class DeferredLightPass : public RenderPass {
		inline static const std::string kSceneAlbedoName = "MainCameraAlbedo";
		inline static const std::string kSceneNormalName = "MainCameraNormal";
		inline static const std::string kScenePositionName = "MainCameraPosition";
		inline static const std::string kSceneMaterialName = "MainCameraMaterial";
		inline static const std::string kSceneShadowMapName = "ShadowMap";
		inline static const std::string kSceneColorName = "MainCameraHdrColor";

		rhi::BufferHandle m_constant_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::SamplerHandle m_sampler{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};

		rhi::TextureHandle m_albedo_target{};
		rhi::TextureHandle m_normal_target{};
		rhi::TextureHandle m_position_target{};
		rhi::TextureHandle m_material_target{};
		rhi::TextureHandle m_shadow_target{};
		rhi::TextureHandle m_skybox_texture{};
		rhi::TextureHandle m_render_target{};
		rhi::FramebufferHandle m_framebuffer{};

	public:
		DeferredLightPass(RhiContext* rhi);
		~DeferredLightPass() override = default;

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onViewportResize(const Vector2i& viewport_extent) override;

	private:
		void createShaders();
		void createBuffers();
		void createSampler();
		void createBindingLayout();
		void createBindingSet();
		void createFramebuffer();
		void createGraphicsPipeline();
	};

} // dodoe
