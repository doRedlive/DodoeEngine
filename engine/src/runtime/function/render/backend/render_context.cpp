//
// Created by Redlive on 2026/3/18.
//

#include "render_context.hpp"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_render_context.h"

namespace dodoe {
    
    Scope<RenderContext> RenderContext::create(RenderContextCreateInfo create_info) {
		Scope<RenderContext> render_context {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			render_context = create_scope<GlRenderContext>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderContext::create: Vulkan backend vertex buffer is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderContext::create: Invalid render api type.");
			break;
		}

		DoAssert(render_context, "RenderContext::create: Create vertex buffer failure.");
		render_context->initialize(create_info);
		return render_context;
	}

	void RenderContext::destroy(Scope<RenderContext>& render_context) {
		if (!render_context) {
			return;
		}

		render_context->shutdown();
		render_context.reset();
	}

} // dodoe