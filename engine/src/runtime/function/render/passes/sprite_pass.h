// Create by Redlive on 2026/4/8.

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "../render_resource.h"

namespace dodoe {
	class DescriptorTableManager;

	class SpritePass : public RenderPass {
		inline static const std::string kInputSceneColorResourceName = "MainCameraColor";
		inline static const std::string kInputSceneDepthResourceName = "MainCameraDepth";

		DescriptorTableManager* m_descriptor_table{nullptr};

		rhi::TextureHandle m_scene_color_target{};
		rhi::TextureHandle m_scene_depth_target{};
		rhi::FramebufferHandle m_framebuffer{};
		rhi::BufferHandle m_vertex_buffer{};
		rhi::BufferHandle m_index_buffer{};
		rhi::BufferHandle m_camera_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::CommandListHandle m_cmd_list{};

		rhi::InputLayoutHandle m_input_layout{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::SamplerHandle m_sampler{};
	public:
		SpritePass(RhiContext* rhi, DescriptorTableManager* descriptor_table);

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onViewportResize(const Vector2i& viewport_extent) override;

	private:
		void createBuffers();
		void createShaders();
		void createSampler();
		void createInputLayout();
		void createBindingLayout();
		void createBindingSet();
		void createFramebuffer();
		void createGraphicsPipeline();
		void drawQuadBatch(const QuadCpuData& batch);
	};

} // dodoe
