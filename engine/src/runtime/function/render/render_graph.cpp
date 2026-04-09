// Created by Redlive on 2026/4/6.

#include "render_graph.h"

#include "passes/main_camera_pass.h"
#include "passes/sprite_pass.h"

#include "runtime/core/utils/common.h"

namespace dodoe {
		
	Scope<RenderGraph> RenderGraph::create(const RenderGraphCreateInfo& info) {
		auto graph = create_scope<RenderGraph>();
		graph->device_ = info.device;
		graph->swapchain_targets_ = info.swapchain_targets;
		graph->target_extent_ = info.target_extent;
		return graph;
	}

	void RenderGraph::destroy(Scope<RenderGraph>& graph) {
		if (!graph) { return; }
		graph->cleanup();
		graph.reset();
	}
	
	void RenderGraph::setup() {
		if (!pass_umap_.empty()) {
			return;
		}

		const identifier main_camera_pass_id = string2hash("main_camera_pass");
		pass_umap_[main_camera_pass_id] = create_scope<MainCameraPass>(
			RenderPassCreateInfo{device_},
			swapchain_targets_,
			target_extent_
		);
		// execute_passes_.push_back(main_camera_pass_id);

		const identifier sprite_pass_id = string2hash("sprite_pass");
		pass_umap_[sprite_pass_id] = create_scope<SpritePass>(
			RenderPassCreateInfo{device_},
			swapchain_targets_,
			target_extent_
		);
		execute_passes_.push_back(sprite_pass_id);
	}

	void RenderGraph::compile() {
		buildDependencies();
		sort();
	}

	void RenderGraph::execute() {
		for (const auto pass_id : execute_passes_) {
			auto it = pass_umap_.find(pass_id);
			if (it == pass_umap_.end() || !it->second) {
				continue;
			}

			it->second->setup();
			it->second->execute();
		}
	}

	void RenderGraph::cleanup() {
		for (const auto pass_id : execute_passes_) {
			auto it = pass_umap_.find(pass_id);
			if (it == pass_umap_.end() || !it->second) {
				continue;
			}

			it->second->cleanup();
		}

		is_compiled_ = false;
	}

	void RenderGraph::buildDependencies() {
		// Stage-3 minimal graph: single pass, no explicit dependencies.
	}

	void RenderGraph::sort() {
		// Stage-3 minimal graph: insertion order is execution order.
	}

}
