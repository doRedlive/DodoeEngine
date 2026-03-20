//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_FRAME_BUFFER_H
#define DODOE_GL_FRAME_BUFFER_H

#include "dopch.h"

#include "runtime/function/render/backend/frame_buffer.h"

namespace dodoe {

	class GlFrameBuffer : public FrameBuffer {
	public:
		void attach() override;
		void detach() override;

	protected:
		void initialize(FrameBufferCreateInfo create_info) override;
		void shutdown() override;

	private:
		uint renderer_id_{0};
		uint color_attachment_id_{0};
		uint depth_stencil_rbo_id_{0};
		ui32 width_{1};
		ui32 height_{1};
	};

} // dodoe

#endif//DODOE_GL_FRAME_BUFFER_H
