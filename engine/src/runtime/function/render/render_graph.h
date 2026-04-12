// Created by Redlive on 2026/4/6.

#pragma once

#include "dopch.h"

#include "render_pass.h"

namespace dodoe {
	class Camera;
	class RhiContext;
	class UiSystem;

	struct RenderGraphCreateInfo {
		rhi::DeviceHandle device{};
		std::vector<rhi::TextureHandle> swapchain_targets{};
		Vector2i target_extent{0, 0};
		Camera* camera{nullptr};
		RhiContext* rhi_backend{nullptr};
		UiSystem* ui_system{nullptr};
	};

	class RenderGraph {
		rhi::DeviceHandle device_{};
		std::vector<rhi::TextureHandle> swapchain_targets_{};
		Vector2i target_extent_{0, 0};
		Camera* camera_{nullptr};
		RhiContext* rhi_backend_{nullptr};
		UiSystem* ui_system_{nullptr};
		bool is_compiled_{false};
		std::vector<identifier> execute_passes_{};
		std::unordered_map<identifier, Scope<RenderPass>> pass_umap_{};
		
	public:
		static Scope<RenderGraph> create(const RenderGraphCreateInfo& info);
		static void destroy(Scope<RenderGraph>& graph);
		
		void setup();
		void compile();
		void execute(uint32_t swapchain_image_index = 0);
		void cleanup();

		const std::vector<rhi::TextureHandle>& getMainSceneTextures();

	private:
		void buildDependencies();
		void sort();
	};

} // dodoe
