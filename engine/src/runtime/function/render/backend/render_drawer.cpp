//
// Created by Redlive on 2026/3/19.
//

#include "render_drawer.h"

#include "runtime/function/render/render_api.h"

#include "glad/glad.h"

namespace dodoe {

	void RenderDrawer::update_viewport(const Rect& viewport) {
		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			glViewport(static_cast<GLint>(viewport.pos.x), static_cast<GLint>(viewport.pos.y), viewport.size.x, viewport.size.y);
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderDrawer::update_viewport: Vulkan backend draw call is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderDrawer::update_viewport: Invalid render api type.");
			break;
		}
	}

	void RenderDrawer::clear_color(const Color& color) {
		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			glClearColor(color.r, color.g, color.b, color.a);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderDrawer::clear_color: Vulkan backend draw call is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderDrawer::clear_color: Invalid render api type.");
			break;
		}
	}

	void RenderDrawer::draw_elements(ui32 index_count) {
		if (index_count == 0) {
			return;
		}

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT, nullptr);
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "RenderDrawer::draw_elements: Vulkan backend draw call is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "RenderDrawer::draw_elements: Invalid render api type.");
			break;
		}
	}

} // dodoe
