//
// Created by Redlive on 2026/3/18.
//

#include "frame_buffer.h"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_frame_buffer.h"

namespace dodoe {

	Scope<FrameBuffer> FrameBuffer::create(FrameBufferCreateInfo create_info) {
		Scope<FrameBuffer> frame_buffer {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			frame_buffer = create_scope<GlFrameBuffer>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "FrameBuffer::create: Vulkan backend frame buffer is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "FrameBuffer::create: Invalid render api type.");
			break;
		}

		DoAssert(frame_buffer, "FrameBuffer::create: Create frame buffer failure.");
		frame_buffer->initialize(create_info);
		return frame_buffer;
	}

	void FrameBuffer::destroy(Scope<FrameBuffer>& frame_buffer) {
		if (!frame_buffer) {
			return;
		}

		frame_buffer->shutdown();
		frame_buffer.reset();
	}

} // dodoe
