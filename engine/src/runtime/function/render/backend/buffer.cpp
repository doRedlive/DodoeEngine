//
// Created by Redlive on 2026/3/18.
//

#include "buffer.h"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_buffer.h"

namespace dodoe {

	Scope<VertexBuffer> VertexBuffer::create(VertexBufferCreateInfo create_info) {
		Scope<VertexBuffer> vertex_buffer {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			vertex_buffer = create_scope<GlVertexBuffer>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "VertexBuffer::create: Vulkan backend vertex buffer is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "VertexBuffer::create: Invalid render api type.");
			break;
		}

		DoAssert(vertex_buffer, "VertexBuffer::create: Create vertex buffer failure.");
		vertex_buffer->initialize(create_info);
		return vertex_buffer;
	}

	void VertexBuffer::destroy(Scope<VertexBuffer>& vertex_buffer) {
		if (!vertex_buffer) {
			return;
		}

		vertex_buffer->shutdown();
		vertex_buffer.reset();
	}

	Scope<IndexBuffer> IndexBuffer::create(IndexBufferCreateInfo create_info) {
		Scope<IndexBuffer> index_buffer {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			index_buffer = create_scope<GlIndexBuffer>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "IndexBuffer::create: Vulkan backend index buffer is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "IndexBuffer::create: Invalid render api type.");
			break;
		}

		DoAssert(index_buffer, "IndexBuffer::create: Create index buffer failure.");
		index_buffer->initialize(create_info);
		return index_buffer;
	}

	void IndexBuffer::destroy(Scope<IndexBuffer>& index_buffer) {
		if (!index_buffer) {
			return;
		}

		index_buffer->shutdown();
		index_buffer.reset();
	}

	Scope<GeometryBinding> GeometryBinding::create(GeometryBindingCreateInfo create_info) {
		Scope<GeometryBinding> geometry_binding{};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			geometry_binding = create_scope<GlGeometryBinding>();
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "GeometryBinding::create: Vulkan backend geometry binding is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "GeometryBinding::create: Invalid render api type.");
			break;
		}

		DoAssert(geometry_binding, "GeometryBinding::create: Create geometry binding failure.");
		geometry_binding->initialize(create_info);
		return geometry_binding;
	}

	void GeometryBinding::destroy(Scope<GeometryBinding>& geometry_binding) {
		if (!geometry_binding) {
			return;
		}

		geometry_binding->shutdown();
		geometry_binding.reset();
	}

} // dodoe
