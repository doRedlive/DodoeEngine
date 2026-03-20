//
// Created by Redlive on 2026/3/18.
//

#include "gl_frame_buffer.h"

#include "glad/glad.h"

namespace dodoe {

	void GlFrameBuffer::initialize(FrameBufferCreateInfo create_info) {
		width_ = std::max<ui32>(1, create_info.width);
		height_ = std::max<ui32>(1, create_info.height);

		glGenFramebuffers(1, reinterpret_cast<GLuint*>(&renderer_id_));
		glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(renderer_id_));

		glGenTextures(1, reinterpret_cast<GLuint*>(&color_attachment_id_));
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(color_attachment_id_));
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			static_cast<GLsizei>(width_),
			static_cast<GLsizei>(height_),
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D,
			static_cast<GLuint>(color_attachment_id_),
			0);

		glGenRenderbuffers(1, reinterpret_cast<GLuint*>(&depth_stencil_rbo_id_));
		glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(depth_stencil_rbo_id_));
		glRenderbufferStorage(
			GL_RENDERBUFFER,
			GL_DEPTH24_STENCIL8,
			static_cast<GLsizei>(width_),
			static_cast<GLsizei>(height_));
		glFramebufferRenderbuffer(
			GL_FRAMEBUFFER,
			GL_DEPTH_STENCIL_ATTACHMENT,
			GL_RENDERBUFFER,
			static_cast<GLuint>(depth_stencil_rbo_id_));

		DoAssert(
			glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"GlFrameBuffer::initialize: Framebuffer is incomplete.");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GlFrameBuffer::shutdown() {
		if (depth_stencil_rbo_id_ != 0) {
			glDeleteRenderbuffers(1, reinterpret_cast<GLuint*>(&depth_stencil_rbo_id_));
			depth_stencil_rbo_id_ = 0;
		}

		if (color_attachment_id_ != 0) {
			glDeleteTextures(1, reinterpret_cast<GLuint*>(&color_attachment_id_));
			color_attachment_id_ = 0;
		}

		if (renderer_id_ != 0) {
			glDeleteFramebuffers(1, reinterpret_cast<GLuint*>(&renderer_id_));
			renderer_id_ = 0;
		}

		width_ = 1;
		height_ = 1;
	}

	void GlFrameBuffer::attach() {
		glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(renderer_id_));
	}

	void GlFrameBuffer::detach() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}

} // dodoe
