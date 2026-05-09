// Created by Redlive on 2026/4/6.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../render_resource.h"
#include "../framework/descriptor_table_manager.h"

#include "runtime/resource/resource_type.h"

namespace dodoe {
	class MainCameraPass : public RenderPass {
		inline static const std::string kSceneAlbedoName = "MainCameraAlbedo";
		inline static const std::string kSceneNormalName = "MainCameraNormal";
		inline static const std::string kScenePositionName = "MainCameraPosition";
		inline static const std::string kSceneMaterialName = "MainCameraMaterial";
		inline static const std::string kSceneDepthName = "MainCameraDepth";

		DescriptorTableManager* m_descriptor_table{nullptr};

		rhi::BufferHandle m_constant_buffer{};
		rhi::ShaderHandle m_vertex_shader{};
		rhi::ShaderHandle m_pixel_shader{};
		rhi::SamplerHandle m_sampler{};
		rhi::InputLayoutHandle m_input_layout{};
		rhi::BindingLayoutHandle m_binding_layout{};
		rhi::BindingSetHandle m_binding_set{};
		rhi::GraphicsPipelineHandle m_graphics_pipeline{};
		rhi::CommandListHandle m_cmd_list{};

		rhi::TextureHandle m_albedo_target{};
		rhi::TextureHandle m_normal_target{};
		rhi::TextureHandle m_position_target{};
		rhi::TextureHandle m_material_target{};
		rhi::TextureHandle m_depth_target{};
		rhi::FramebufferHandle m_framebuffer{};
	public:
		MainCameraPass(RhiContext* rhi, DescriptorTableManager* descriptor_manager);
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
		ui32 resolveTextureIndex(const Ref<MeshGeometry>& geometry) const;
		ui32 resolveMetallicRoughnessTextureIndex(const Ref<MeshGeometry>& geometry) const;
	};

} // dodoe
