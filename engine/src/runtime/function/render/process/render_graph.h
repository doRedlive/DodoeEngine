//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_GRAPH_H
#define DODOE_RENDER_GRAPH_H

#include "dopch.h"

#include "render_stage.h"

#include "../camera/camera.h"

namespace dodoe {

	struct RenderGraphCreateInfo {
	};

	class RenderGraph {
	public:
		virtual ~RenderGraph() = default;

		template <typename TGraph>
		static Scope<RenderGraph> create(const RenderGraphCreateInfo& create_info) {
			static_assert(std::is_base_of_v<RenderGraph, TGraph>, "Error: T must inherit from RenderGraph");
			auto graph = create_scope<TGraph>();
			graph->initialize(create_info);
			return graph;
		}

		static void destroy(Scope<RenderGraph>& render_graph) {
			if (!render_graph) { return;	}
			render_graph->shutdown();
			render_graph.reset();
		}

		virtual void prepare() = 0;
		virtual void flush() = 0;

		virtual void initialize(const RenderGraphCreateInfo& create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_RENDER_GRAPH_H
