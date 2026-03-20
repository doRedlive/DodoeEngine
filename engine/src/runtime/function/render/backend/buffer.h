//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_BUFFER_H
#define DODOE_BUFFER_H

#include "dopch.h"

namespace dodoe {

	struct VertexAttribute {
		uint location{0};
		uint offset{0};
		uint stride{0};
		uint components{0};
		bool normalized{false};
	};

	struct VertexLayout {
		std::vector<VertexAttribute> attributes;
	};

	struct VertexBufferCreateInfo {
		const void* data{nullptr};
		ui32 size{0};
		bool dynamic{false};
	};

	class VertexBuffer {
	public:
		virtual ~VertexBuffer() = default;

		static Scope<VertexBuffer> create(VertexBufferCreateInfo create_info);
		static void destroy(Scope<VertexBuffer>& vertex_buffer);
		virtual void upload_data(const void* data, ui32 size, ui32 offset = 0) = 0;

	protected:
		virtual void initialize(VertexBufferCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

	struct IndexBufferCreateInfo {
		const void* data{nullptr};
		ui32 size{0};
		bool dynamic{false};
	};

	class IndexBuffer {
	public:
		virtual ~IndexBuffer() = default;

		static Scope<IndexBuffer> create(IndexBufferCreateInfo create_info);
		static void destroy(Scope<IndexBuffer>& index_buffer);
		virtual void upload_data(const void* data, ui32 size, ui32 offset = 0) = 0;

	protected:
		virtual void initialize(IndexBufferCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

	struct GeometryBindingCreateInfo {
		VertexBuffer* vertex_buffer{nullptr};
		IndexBuffer* index_buffer{nullptr};
		VertexLayout vertex_layout{};
	};

	class GeometryBinding {
	public:
		virtual ~GeometryBinding() = default;

		static Scope<GeometryBinding> create(GeometryBindingCreateInfo create_info);
		static void destroy(Scope<GeometryBinding>& geometry_binding);

		virtual void bind() = 0;

	protected:
		virtual void initialize(GeometryBindingCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_BUFFER_H
