//
// Created by Redlive on 2026/3/17.
//

#include "render_graph.h"

namespace dodoe {

	Scope<RenderGraph> RenderGraph::create(RenderGraphCreateInfo create_info) {
		auto context = create_scope<RenderGraph>();
		context->initialize(create_info);
		return context;
	}

	void RenderGraph::destroy(Scope<RenderGraph>& render_graph) {
		if (!render_graph) {
			return;
		}

		render_graph->shutdown();
		render_graph.reset();
	}

	void RenderGraph::initialize(RenderGraphCreateInfo create_info) {
		sprite_stage = RenderStage::create({create_info.framebuffer_width, create_info.framebuffer_height, create_info.camera});
	}

	void RenderGraph::shutdown() {
		RenderStage::destroy(sprite_stage);
	}

} // dodoe
