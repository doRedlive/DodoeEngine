//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_GRAPH_H
#define DODOE_RENDER_GRAPH_H

#include "dopch.h"

#include "render_stage.h"

#include "camera/camera.h"

namespace dodoe {

	struct RenderGraphCreateInfo {
		ui32 framebuffer_width{1};
		ui32 framebuffer_height{1};
		Camera* camera{nullptr};

		RenderGraphCreateInfo() = default;
		RenderGraphCreateInfo(const Vector2f& logical_size, Camera* cam) : framebuffer_width(static_cast<uint>(logical_size.x)), framebuffer_height(static_cast<uint>(logical_size.y)), camera(cam) { }
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
