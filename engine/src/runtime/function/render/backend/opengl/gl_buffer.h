//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_BUFFER_H
#define DODOE_GL_BUFFER_H

#include "dopch.h"

#include "runtime/function/render/backend/buffer.h"

namespace dodoe {

	class GlVertexBuffer : public VertexBuffer {
	public:
		uint renderer_id() const { return renderer_id_; }
		void upload_data(const void* data, ui32 size, ui32 offset = 0) override;

	protected:
		void initialize(VertexBufferCreateInfo create_info) override;
		void shutdown() override;

	private:
		uint renderer_id_{0};
		ui32 buffer_size_{0};
	};

	class GlIndexBuffer : public IndexBuffer {
	public:
		uint renderer_id() const { return renderer_id_; }
		void upload_data(const void* data, ui32 size, ui32 offset = 0) override;

	protected:
		void initialize(IndexBufferCreateInfo create_info) override;
		void shutdown() override;

	private:
		uint renderer_id_{0};
		ui32 buffer_size_{0};
	};

	class GlGeometryBinding : public GeometryBinding {
	public:
		void bind() override;

	protected:
		void initialize(GeometryBindingCreateInfo create_info) override;
		void shutdown() override;

	private:
		uint vertex_array_id_{0};
	};

} // dodoe

#endif//DODOE_GL_BUFFER_H
