//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDER_BATCH_H
#define DODOE_RENDER_BATCH_H

#include "dopch.h"

#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/backend/buffer.h"
#include "runtime/function/render/backend/texture.h"

namespace dodoe {
	struct RenderBatchPacket {
		std::vector<uchar> vertex_data;
		std::vector<Ref<Texture>> texture_slots;
		ui32 index_count{0};
	};

	struct RenderBatchCreateInfo {

	};

	class RenderBatch {
		struct QuadVertex {
			Vector3f position;
			Vector2f uv;
			Vector4f color;
			float texture_index;
		};
	public:
		static Scope<RenderBatch> create(RenderBatchCreateInfo create_info);
		static void destroy(Scope<RenderBatch>& render_batch);

		void flush();
		void queue_draw_context(const QuadDrawContext& context);
		
	private:
		const uint max_quads_{20000};
		const uint max_texslots_{16};
		const uint max_indices_{20000 * 6};
		const uint max_vertices_{20000 * 4};

		Ref<Texture> default_texture_{nullptr};
		std::vector<QuadDrawContext> quad_draw_contexts_{};	
		std::vector<QuadVertex> quad_vertices_;
		Scope<VertexBuffer> quad_vb_;
		Scope<IndexBuffer> quad_ib_;
		Scope<GeometryBinding> geometry_binding_;
		
		std::vector<RenderBatchPacket> build_batches();

		void initialize(RenderBatchCreateInfo create_info);
		void shutdown();
	};

} // dodoe

#endif//DODOE_RENDER_BATCH_H
