//
// Created by Redlive on 2026/3/18.
//

#include "render_process.h"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_render_process.h"

namespace dodoe {

	Scope<RenderPass> RenderPass::create(RenderPassCreateInfo create_info) {
		Scope<RenderPass> render_pass{};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			render_pass = create_scope<GlRenderPass>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderPass::create: Vulkan backend render pass is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderPass::create: Invalid render api type.");
			break;
		}

		DoAssert(render_pass, "RenderPass::create: Create render pass failure.");
		render_pass->initialize(create_info);
		return render_pass;
	}

	void RenderPass::destroy(Scope<RenderPass>& render_pass) {
		if (!render_pass) {
			return;
		}

		render_pass->shutdown();
		render_pass.reset();
	}

	Scope<RenderPipeline> RenderPipeline::create(RenderPipelineCreateInfo create_info) {
		Scope<RenderPipeline> render_pipeline{};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			render_pipeline = create_scope<GlRenderPipeline>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderPipeline::create: Vulkan backend render pipeline is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderPipeline::create: Invalid render api type.");
			break;
		}

		DoAssert(render_pipeline, "RenderPipeline::create: Create render pipeline failure.");
		render_pipeline->initialize(create_info);
		return render_pipeline;
	}

	void RenderPipeline::destroy(Scope<RenderPipeline>& render_pipeline) {
		if (!render_pipeline) {
			return;
		}

		render_pipeline->shutdown();
		render_pipeline.reset();
	}

} // dodoe
