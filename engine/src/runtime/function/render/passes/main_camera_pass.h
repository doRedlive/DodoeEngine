// Created by Redlive on 2026/4/6.

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"

#include "runtime/resource/resource_type.h"

namespace dodoe {

	class MainCameraPass : public RenderPass {
		std::vector<rhi::TextureHandle> swapchain_targets_{};
		Vector2i target_extent_{0, 0};

		std::vector<rhi::FramebufferHandle> framebuffers_{};
		rhi::GraphicsPipelineHandle graphics_pipeline_{};
		rhi::BufferHandle constant_buffer_{};
		rhi::BufferHandle vertex_buffer_{};
		rhi::BindingLayoutHandle binding_layout_{};
		rhi::BindingSetHandle binding_set_{};
		rhi::CommandListHandle cmd_list_{};

		struct MainCameraVertex {
			Vector3f position{0.0f};
			Vector3f normal{0.0f, 0.0f, 1.0f};
		};

		std::vector<MainCameraVertex> draw_vertices_{};
		bool initialized_{false};
		bool mesh_loaded_{false};
		identifier model_id_{0};
	public:
		MainCameraPass(const RenderPassCreateInfo& info, const std::vector<rhi::TextureHandle>& swapchain_targets, const Vector2i& target_extent);
		~MainCameraPass() override = default;

		void setup() override;
		void execute(size_t index) override;
		void cleanup() override;

	private:
		void createFramebuffers();
		void createGraphicsPipeline();
		void createBuffers();
		void fillResources();
		void loadMeshFromResourceManager();
	};

} // dodoe