//
// Created by Redlive on 2026/3/17.
//

#include "render_batch.h"

#include "runtime/function/render/backend/render_drawer.h"
#include "runtime/core/math/math.h"

#include <cmath>
#include <cstring>

namespace dodoe {

	Scope<RenderBatch> RenderBatch::create(RenderBatchCreateInfo create_info) {
		auto context = create_scope<RenderBatch>();
		context->initialize(create_info);
		return context;
	}

	void RenderBatch::destroy(Scope<RenderBatch>& render_batch) {
		if (!render_batch) { return; }
		render_batch->shutdown();
		render_batch.reset();
	}

	void RenderBatch::initialize(RenderBatchCreateInfo create_info) {
		(void)create_info;
		quad_draw_contexts_.reserve(max_quads_);
		quad_vertices_.reserve(max_vertices_);

		if (!default_texture_) {
			std::array<uchar, 4> white_pixel{255, 255, 255, 255};
			default_texture_ = Texture::create({1, 1, white_pixel.data()});
		}

		quad_vb_ = VertexBuffer::create(
			{nullptr, static_cast<ui32>(max_vertices_ * sizeof(QuadVertex)), true});

		std::vector<ui32> quad_indices;
		quad_indices.reserve(max_indices_);
		for (ui32 i = 0; i < max_quads_; ++i) {
			const ui32 base = i * 4;
			quad_indices.push_back(base + 0);
			quad_indices.push_back(base + 1);
			quad_indices.push_back(base + 2);
			quad_indices.push_back(base + 2);
			quad_indices.push_back(base + 3);
			quad_indices.push_back(base + 0);
		}

		quad_ib_ = IndexBuffer::create(
			{quad_indices.data(), static_cast<ui32>(quad_indices.size() * sizeof(ui32)), false});

		VertexLayout layout;
		layout.attributes = {
			{0, static_cast<uint>(offsetof(QuadVertex, position)), static_cast<uint>(sizeof(QuadVertex)), 3, false},
			{1, static_cast<uint>(offsetof(QuadVertex, uv)), static_cast<uint>(sizeof(QuadVertex)), 2, false},
			{2, static_cast<uint>(offsetof(QuadVertex, color)), static_cast<uint>(sizeof(QuadVertex)), 4, false},
			{3, static_cast<uint>(offsetof(QuadVertex, texture_index)), static_cast<uint>(sizeof(QuadVertex)), 1, false},
		};

		geometry_binding_ = GeometryBinding::create({quad_vb_.get(), quad_ib_.get(), layout});
	}

	void RenderBatch::shutdown() {
		quad_draw_contexts_.clear();
		quad_vertices_.clear();
		default_texture_.reset();

		GeometryBinding::destroy(geometry_binding_);
		VertexBuffer::destroy(quad_vb_);
		IndexBuffer::destroy(quad_ib_);
	}

	void RenderBatch::queue_draw_context(const QuadDrawContext& context) {
		quad_draw_contexts_.push_back(context);
	}

	void RenderBatch::flush() {
		if (!quad_vb_ || !geometry_binding_) {
			return;
		}

		auto batches = build_batches();
		geometry_binding_->bind();

		for (const auto& batch : batches) {
			if (batch.vertex_data.empty() || batch.index_count == 0) {
				continue;
			}

			quad_vb_->upload_data(batch.vertex_data.data(), static_cast<ui32>(batch.vertex_data.size()));
			for (size_t slot = 0; slot < batch.texture_slots.size(); ++slot) {
				if (!batch.texture_slots[slot]) {
					continue;
				}

				batch.texture_slots[slot]->attach(static_cast<uint>(slot));
			}
			RenderDrawer::draw_elements(batch.index_count);
		}
	}

	std::vector<RenderBatchPacket> RenderBatch::build_batches() {
		std::vector<RenderBatchPacket> out_batches;

		if (quad_draw_contexts_.empty()) {
			return out_batches;
		}

		size_t cursor = 0;
		while (cursor < quad_draw_contexts_.size()) {
			quad_vertices_.clear();
			quad_vertices_.reserve(max_vertices_);

			std::unordered_map<Texture*, ui32> texture_slot_lookup;
			std::vector<Ref<Texture>> texture_slots;
			texture_slot_lookup.reserve(max_texslots_);
			texture_slots.reserve(max_texslots_);

			size_t batch_quad_count = 0;
			size_t batch_end = cursor;
			for (; batch_end < quad_draw_contexts_.size() && batch_quad_count < max_quads_; ++batch_end) {
				const auto& draw_context = quad_draw_contexts_[batch_end];
				const Ref<Texture> texture_ref = draw_context.texture ? draw_context.texture : default_texture_;
				if (!texture_ref) {
					continue;
				}

				Texture* texture_ptr = texture_ref.get();
				auto texture_it = texture_slot_lookup.find(texture_ptr);
				ui32 texture_slot_index = 0;
				if (texture_it == texture_slot_lookup.end()) {
					if (texture_slots.size() >= max_texslots_) {
						break;
					}

					texture_slot_index = static_cast<ui32>(texture_slots.size());
					texture_slot_lookup.emplace(texture_ptr, texture_slot_index);
					texture_slots.push_back(texture_ref);
				} else {
					texture_slot_index = texture_it->second;
				}

				const float x = draw_context.dst_rect.x;
				const float y = draw_context.dst_rect.y;
				const float w = draw_context.dst_rect.z;
				const float h = draw_context.dst_rect.w;

				const float cx = x + w * 0.5f;
				const float cy = y + h * 0.5f;
				const float hx = w * 0.5f;
				const float hy = h * 0.5f;

				const float r = Math::radians(draw_context.rotation.z);
				const float cos_r = std::cos(r);
				const float sin_r = std::sin(r);

				auto rotate_and_translate = [&](float lx, float ly) -> Vector3f {
					return Vector3f(cx + lx * cos_r - ly * sin_r, cy + lx * sin_r + ly * cos_r, 0.0f);
				};

				const float u0 = draw_context.uv_rect.x;
				const float v0 = draw_context.uv_rect.y;
				const float u1 = draw_context.uv_rect.z;
				const float v1 = draw_context.uv_rect.w;
				const float tex_index = static_cast<float>(texture_slot_index);

				quad_vertices_.push_back({rotate_and_translate(-hx, -hy), Vector2f(u0, v0), draw_context.color, tex_index});
				quad_vertices_.push_back({rotate_and_translate( hx, -hy), Vector2f(u1, v0), draw_context.color, tex_index});
				quad_vertices_.push_back({rotate_and_translate( hx,  hy), Vector2f(u1, v1), draw_context.color, tex_index});
				quad_vertices_.push_back({rotate_and_translate(-hx,  hy), Vector2f(u0, v1), draw_context.color, tex_index});

				++batch_quad_count;
			}

			if (batch_quad_count == 0) {
				break;
			}

			RenderBatchPacket packet;
			packet.index_count = static_cast<ui32>(batch_quad_count * 6);
			packet.texture_slots = std::move(texture_slots);

			const ui32 vertex_bytes = static_cast<ui32>(quad_vertices_.size() * sizeof(QuadVertex));
			packet.vertex_data.resize(vertex_bytes);
			memcpy(packet.vertex_data.data(), quad_vertices_.data(), vertex_bytes);

			out_batches.push_back(std::move(packet));

			cursor = batch_end;
		}

		quad_draw_contexts_.clear();
		return out_batches;
	}

} // dodoe
