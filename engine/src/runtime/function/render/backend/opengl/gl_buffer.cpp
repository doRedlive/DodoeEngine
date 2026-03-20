//
// Created by Redlive on 2026/3/18.
//

#include "gl_buffer.h"

#include "glad/glad.h"

namespace dodoe {

	void GlVertexBuffer::initialize(VertexBufferCreateInfo create_info) {
		glGenBuffers(1, reinterpret_cast<GLuint*>(&renderer_id_));
		DoAssert(renderer_id_ != 0, "GlVertexBuffer::initialize: Failed to create OpenGL vertex buffer.");
		buffer_size_ = create_info.size;

		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(renderer_id_));
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(create_info.size), create_info.data,
			create_info.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void GlVertexBuffer::upload_data(const void* data, ui32 size, ui32 offset) {
		DoAssert(renderer_id_ != 0, "GlVertexBuffer::upload_data: Buffer is not initialized.");
		DoAssert(offset + size <= buffer_size_, "GlVertexBuffer::upload_data: Upload range out of bounds.");

		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(renderer_id_));
		glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void GlVertexBuffer::shutdown() {
		if (renderer_id_ == 0) {
			return;
		}

		glDeleteBuffers(1, reinterpret_cast<const GLuint*>(&renderer_id_));
		renderer_id_ = 0;
		buffer_size_ = 0;
	}

	void GlIndexBuffer::initialize(IndexBufferCreateInfo create_info) {
		glGenBuffers(1, reinterpret_cast<GLuint*>(&renderer_id_));
		DoAssert(renderer_id_ != 0, "GlIndexBuffer::initialize: Failed to create OpenGL index buffer.");
		buffer_size_ = create_info.size;

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(renderer_id_));
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(create_info.size), create_info.data,
			create_info.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void GlIndexBuffer::upload_data(const void* data, ui32 size, ui32 offset) {
		DoAssert(renderer_id_ != 0, "GlIndexBuffer::upload_data: Buffer is not initialized.");
		DoAssert(offset + size <= buffer_size_, "GlIndexBuffer::upload_data: Upload range out of bounds.");

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(renderer_id_));
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void GlIndexBuffer::shutdown() {
		if (renderer_id_ == 0) {
			return;
		}

		glDeleteBuffers(1, reinterpret_cast<const GLuint*>(&renderer_id_));
		renderer_id_ = 0;
		buffer_size_ = 0;

	}

	void GlGeometryBinding::bind() {
		DoAssert(vertex_array_id_ != 0, "GlGeometryBinding::bind: Vertex array object is not initialized.");
		glBindVertexArray(static_cast<GLuint>(vertex_array_id_));

	}

	void GlGeometryBinding::initialize(GeometryBindingCreateInfo create_info) {
		DoAssert(create_info.vertex_buffer, "GeometryBindingCreateInfo::vertex_buffer must not be null.");
		DoAssert(create_info.index_buffer, "GeometryBindingCreateInfo::index_buffer must not be null.");

		auto* vertex_buffer = dynamic_cast<GlVertexBuffer*>(create_info.vertex_buffer);
		auto* index_buffer = dynamic_cast<GlIndexBuffer*>(create_info.index_buffer);

		DoAssert(vertex_buffer, "GlGeometryBinding::initialize: vertex buffer must be GlVertexBuffer.");
		DoAssert(index_buffer, "GlGeometryBinding::initialize: index buffer must be GlIndexBuffer.");
		DoAssert(vertex_buffer->renderer_id() != 0,
			"GlGeometryBinding::initialize: vertex buffer is not initialized.");
		DoAssert(index_buffer->renderer_id() != 0,
			"GlGeometryBinding::initialize: index buffer is not initialized.");

		glGenVertexArrays(1, reinterpret_cast<GLuint*>(&vertex_array_id_));
		DoAssert(vertex_array_id_ != 0, "GlGeometryBinding::initialize: Failed to create OpenGL vertex array.");

		glBindVertexArray(static_cast<GLuint>(vertex_array_id_));
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vertex_buffer->renderer_id()));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(index_buffer->renderer_id()));

		for (const auto& attribute : create_info.vertex_layout.attributes) {
			DoAssert(attribute.components > 0,
				"GlGeometryBinding::initialize: attribute.components must be greater than zero.");
			glEnableVertexAttribArray(static_cast<GLuint>(attribute.location));
			glVertexAttribPointer(static_cast<GLuint>(attribute.location), static_cast<GLint>(attribute.components),
				GL_FLOAT, attribute.normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(attribute.stride),
				reinterpret_cast<const void*>(static_cast<uintptr_t>(attribute.offset)));
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void GlGeometryBinding::shutdown() {
		if (vertex_array_id_ == 0) {
			return;
		}

		glDeleteVertexArrays(1, reinterpret_cast<const GLuint*>(&vertex_array_id_));
		vertex_array_id_ = 0;

	}

} // dodoe
