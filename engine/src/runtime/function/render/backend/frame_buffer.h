//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_FRAME_BUFFER_H
#define DODOE_FRAME_BUFFER_H

#include "dopch.h"

namespace dodoe {

	struct FrameBufferCreateInfo {
		ui32 width{1};
		ui32 height{1};
	};

	class FrameBuffer {
	public:
		virtual ~FrameBuffer() = default;

		static Scope<FrameBuffer> create(FrameBufferCreateInfo create_info);
		static void destroy(Scope<FrameBuffer>& frame_buffer);
		virtual void attach() = 0;
		virtual void detach() = 0;

	protected:
		virtual void initialize(FrameBufferCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_FRAME_BUFFER_H
