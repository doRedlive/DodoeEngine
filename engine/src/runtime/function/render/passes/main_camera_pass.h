// Created by Redlive on 2026/4/6.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../render_resource.h"
#include "../framework/descriptor_table_manager.h"

#include "runtime/resource/resource_type.h"

namespace dodoe {
	class Camera;

	class MainCameraPass : public RenderPass {
		inline static const std::string kSceneColorName = "MainCameraColor";
		inline static const std::string kSceneDepthName = "MainCameraDepth";

		Camera* m_camera{nullptr};
		DescriptorTableManager* m_descriptor_table{nullptr};

		rhi::BufferHandle m_constant_buffer{};
		rhi::BufferHandle m_vertex_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::SamplerHandle m_sampler{};
		rhi::InputLayoutHandle m_input_layout{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};

		rhi::TextureHandle m_render_target{};
		rhi::TextureHandle m_depth_target{};
		rhi::FramebufferHandle m_framebuffer{};

		struct MainCameraVertex {
			Vector3f position{0.0f};
			Vector3f normal{0.0f, 0.0f, 1.0f};
			Vector2f uv{0.0f, 0.0f};
			ui32 texture_index{0};
		};

		std::vector<MainCameraVertex> draw_vertices_{};
		std::unordered_map<identifier, std::vector<MainCameraVertex>> model_vertex_cache_{};
	public:
		MainCameraPass(RhiContext* rhi, Camera* camera, DescriptorTableManager* descriptor_manager);
		~MainCameraPass() override = default;

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;
		void onViewportResize(const Vector2i& viewport_extent) override;

	private:
		void createShaders();
		void createBuffers();
		void createSampler();
		void createInputLayout();
		void createBindingLayout();
		void createBindingSet();
		void createFramebuffer();
		void createGraphicsPipeline();

		void rebuildDrawVerticesFromScene();
		void appendModelVertices(const MainCameraDrawPacket& packet, std::vector<MainCameraVertex>& out_vertices);
	};

} // dodoe
