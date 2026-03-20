//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_GRAPH_H
#define DODOE_RENDER_GRAPH_H

#include "dopch.h"

#include "render_stage.h"

namespace dodoe {

	struct RenderGraphCreateInfo {
		ui32 frame_width{1};
		ui32 frame_height{1};
	};

	class RenderGraph {
	public:
		Scope<RenderStage> sprite_stage;
		static Scope<RenderGraph> create(RenderGraphCreateInfo create_info);
		static void destroy(Scope<RenderGraph>& render_graph);

	private:
		void initialize(RenderGraphCreateInfo create_info);
		void shutdown();
	};

} // dodoe

#endif//DODOE_RENDER_GRAPH_H
