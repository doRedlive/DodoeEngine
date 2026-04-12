// Created by Redlive on 2026/4/6.

#include "render_graph.h"

#include "passes/imgui_pass.h"
#include "passes/main_camera_pass.h"
#include "passes/sprite_pass.h"

#include "runtime/core/utils/common.h"

namespace dodoe {
		
	Scope<RenderGraph> RenderGraph::create(const RenderGraphCreateInfo& info) {
		auto graph = create_scope<RenderGraph>();
		graph->device_ = info.device;
		graph->swapchain_targets_ = info.swapchain_targets;
		graph->target_extent_ = info.target_extent;
		graph->camera_ = info.camera;
		graph->rhi_backend_ = info.rhi_backend;
		graph->ui_system_ = info.ui_system;
		return graph;
	}

	void RenderGraph::destroy(Scope<RenderGraph>& graph) {
		if (!graph) { return; }
		graph->cleanup();
		graph.reset();
	}
	
	void RenderGraph::setup() {
		// const identifier main_camera_pass_id = string2hash("main_camera_pass");
		// pass_umap_[main_camera_pass_id] = create_scope<MainCameraPass>(
		// 	RenderPassCreateInfo{device_},
		// 	swapchain_targets_,
		// 	target_extent_
		// );
		// execute_passes_.push_back(main_camera_pass_id);

		const identifier sprite_pass_id = string2hash("sprite_pass");
		pass_umap_[sprite_pass_id] = create_scope<SpritePass>(
			RenderPassCreateInfo{device_},
			swapchain_targets_.size(),
			target_extent_,
			camera_
		);
		execute_passes_.push_back(sprite_pass_id);

		const identifier imgui_pass_id = string2hash("imgui_pass");
		pass_umap_[imgui_pass_id] = create_scope<ImGuiPass>(
			RenderPassCreateInfo{device_}, 
			rhi_backend_, 
			swapchain_targets_, 
			target_extent_
		);
		execute_passes_.push_back(imgui_pass_id);
	}

	void RenderGraph::compile() {
		buildDependencies();
		sort();

		const identifier sprite_pass_id = string2hash("sprite_pass");
		auto sprite_it = pass_umap_.find(sprite_pass_id);
		if (sprite_it != pass_umap_.end() && sprite_it->second) {
			sprite_it->second->setup();
		}

		for (const auto pass_id : execute_passes_) {
			if (pass_id == sprite_pass_id) {
				continue;
			}
			if (!pass_umap_.count(pass_id)) continue;
			pass_umap_[pass_id]->setup();
		}
	}

	void RenderGraph::execute(uint32_t swapchain_image_index) {
		for (const auto pass_id : execute_passes_) {
			if (!pass_umap_.count(pass_id)) continue;
			pass_umap_[pass_id]->execute(swapchain_image_index);
		}
	}

	void RenderGraph::cleanup() {
		for (const auto pass_id : execute_passes_) {
			if (!pass_umap_.count(pass_id)) continue;
			pass_umap_[pass_id]->cleanup();
		}
	}

	const std::vector<rhi::TextureHandle>& RenderGraph::getMainSceneTextures() {
		auto sprite_pass = dynamic_cast<SpritePass*>(pass_umap_[string2hash("sprite_pass")].get());
		return sprite_pass->scene_targets();
	}

	void RenderGraph::buildDependencies() {
		// Stage-3 minimal graph: single pass, no explicit dependencies.
	}

	void RenderGraph::sort() {
		// Stage-3 minimal graph: insertion order is execution order.
	}

}
