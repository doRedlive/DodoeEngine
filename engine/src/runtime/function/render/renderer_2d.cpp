//
// Created by Redlive 2026/3/17.
//

#include "renderer_2d.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    namespace {

        struct QuadDrawContext {
            identifier texture_id{0};
            Vector4f dst_rect{0.0f};
            Vector4f uv_rect{0.0f};
            Vector3f rotation{0.0f};
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        };

        struct LineDrawContext {
            Vector2f start{0.0f};
            Vector2f end{0.0f};
            Vector3f rotation{0.0f};
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
            float thickness{2.0f};
        };

        struct TextDrawContext {
            identifier texture_id{0};
            Vector4f dst_rect{0.0f};
            Vector4f uv_rect{0.0f};
            Vector3f rotation{0.0f};
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        };

        struct Renderer2dSubmitLists {
            std::vector<QuadDrawContext> quads{};
            std::vector<LineDrawContext> lines{};
            std::vector<TextDrawContext> texts{};

            void clear() {
                quads.clear();
                lines.clear();
                texts.clear();
            }
        };

        struct Renderer2dState {
            Renderer2dSubmitLists submit{};
            std::vector<QuadCpuData> quad_batches{};
            bool dirty{false};

            void clear() {
                submit.clear();
                quad_batches.clear();
                dirty = false;
            }
        };

        static Renderer2dState s_Data;

        void pushQuadData(
            QuadCpuData& out,
            const Vector4f& dst_rect,
            const Vector4f& uv_rect,
            const Vector3f& rotation,
            const Vector4f& color,
            const ui32 texture_index
        ) {
            const float x = dst_rect.x;
            const float y = dst_rect.y;
            const float w = dst_rect.z;
            const float h = dst_rect.w;

            const float cx = x + w * 0.5f;
            const float cy = y + h * 0.5f;
            const float hx = w * 0.5f;
            const float hy = h * 0.5f;

            const float radians = rotation.z * (Math::PI / 180.0f);
            const float cos_r = std::cos(radians);
            const float sin_r = std::sin(radians);

            auto rotate_and_translate = [&](float lx, float ly) -> Vector3f {
                return Vector3f(
                    cx + lx * cos_r - ly * sin_r,
                    cy + lx * sin_r + ly * cos_r,
                    0.0f
                );
            };

            const float u0 = uv_rect.x;
            const float v0 = uv_rect.y;
            const float u1 = uv_rect.z;
            const float v1 = uv_rect.w;

            const ui32 base_vertex = static_cast<ui32>(out.vertices.size());
            out.vertices.push_back({rotate_and_translate(-hx, -hy), {u0, v0}, color, texture_index});
            out.vertices.push_back({rotate_and_translate( hx, -hy), {u1, v0}, color, texture_index});
            out.vertices.push_back({rotate_and_translate( hx,  hy), {u1, v1}, color, texture_index});
            out.vertices.push_back({rotate_and_translate(-hx,  hy), {u0, v1}, color, texture_index});

            out.indices.push_back(base_vertex + 0);
            out.indices.push_back(base_vertex + 1);
            out.indices.push_back(base_vertex + 2);
            out.indices.push_back(base_vertex + 2);
            out.indices.push_back(base_vertex + 3);
            out.indices.push_back(base_vertex + 0);
        }

        struct QuadBatchBuilder {
            QuadCpuData batch{};
            std::unordered_map<identifier, ui32> texture_index{};

            size_t quadCount() const {
                return batch.vertices.size() / 4;
            }

            bool empty() const {
                return batch.vertices.empty() || batch.indices.empty();
            }

            void reset() {
                batch.clear();
                texture_index.clear();
            }

            ui32 resolveTextureIndex(identifier texture_id) {
                auto slot_it = texture_index.find(texture_id);
                if (slot_it != texture_index.end()) {
                    return slot_it->second;
                }

                const ui32 texture_index_value = static_cast<ui32>(batch.textures.size());
                texture_index.emplace(texture_id, texture_index_value);
                batch.textures.push_back(texture_id);
                return texture_index_value;
            }

            bool wouldExceedTextureLimit(identifier texture_id) const {
                if (texture_index.find(texture_id) != texture_index.end()) {
                    return false;
                }
                return (batch.textures.size() + 1) > static_cast<size_t>(Renderer2d::MaxTextureCount);
            }

            void pushQuad(
                identifier texture_id,
                const Vector4f& dst_rect,
                const Vector4f& uv_rect,
                const Vector3f& rotation,
                const Vector4f& color
            ) {
                const ui32 texture_index_value = resolveTextureIndex(texture_id);
                pushQuadData(batch, dst_rect, uv_rect, rotation, color, texture_index_value);
            }
        };

        void flushIfNotEmpty(std::vector<QuadCpuData>& out, QuadBatchBuilder& builder) {
            if (builder.empty()) {
                builder.reset();
                return;
            }
            out.push_back(std::move(builder.batch));
            builder.reset();
        }

        void buildQuadBatches(std::vector<QuadCpuData>& out, const Renderer2dSubmitLists& submit) {
            out.clear();
            if (submit.quads.empty() && submit.lines.empty()) {
                return;
            }

            QuadBatchBuilder builder{};
            builder.batch.vertices.reserve((std::min)(submit.quads.size() + submit.lines.size(), static_cast<size_t>(Renderer2d::MaxQuadCount)) * 4);
            builder.batch.indices.reserve((std::min)(submit.quads.size() + submit.lines.size(), static_cast<size_t>(Renderer2d::MaxQuadCount)) * 6);
            builder.batch.textures.reserve((std::min)(submit.quads.size() + submit.lines.size(), static_cast<size_t>(Renderer2d::MaxTextureCount)));
            builder.texture_index.reserve((std::min)(submit.quads.size() + submit.lines.size(), static_cast<size_t>(Renderer2d::MaxTextureCount)));

            auto ensureRoomForQuad = [&](identifier texture_id) {
                const bool quad_limit_hit = builder.quadCount() >= static_cast<size_t>(Renderer2d::MaxQuadCount);
                const bool texture_limit_hit = builder.wouldExceedTextureLimit(texture_id);
                if ((quad_limit_hit || texture_limit_hit) && !builder.empty()) {
                    flushIfNotEmpty(out, builder);
                }
            };

            for (const auto& quad : submit.quads) {
                ensureRoomForQuad(quad.texture_id);
                builder.pushQuad(quad.texture_id, quad.dst_rect, quad.uv_rect, quad.rotation, quad.color);
            }

            for (const auto& line : submit.lines) {
                if (line.thickness <= 0.0f) {
                    continue;
                }

                const Vector2f delta = line.end - line.start;
                const float length = Math::length(delta);
                const float half_thickness = line.thickness * 0.5f;

                Vector4f rect{};
                Vector3f rotation{0.0f, 0.0f, 0.0f};

                if (length <= std::numeric_limits<float>::epsilon()) {
                    rect = {
                        line.start.x - half_thickness,
                        line.start.y - half_thickness,
                        line.thickness,
                        line.thickness
                    };
                } else {
                    const Vector2f center = (line.start + line.end) * 0.5f;
                    rect = {
                        center.x - length * 0.5f,
                        center.y - half_thickness,
                        length,
                        line.thickness
                    };
                    rotation.z = Math::rad2deg(std::atan2(delta.y, delta.x));
                }

                rotation += line.rotation;

                ensureRoomForQuad(0);
                builder.pushQuad(0, rect, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, line.color);
            }

            flushIfNotEmpty(out, builder);
        }
    }

    void Renderer2d::drawSprite(const identifier texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Color& color) {
        if (!texture) {
            DoError("Renderer2d::drawSprite: texture is null.");
            return;
        }

        QuadDrawContext quad{};
        quad.texture_id = texture;
        quad.dst_rect = {pos.x, pos.y, size.x, size.y};
        quad.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        quad.rotation = rotation;
        quad.color = color.to_vec4();

        s_Data.submit.quads.push_back(quad);
        s_Data.dirty = true;
    }

    void Renderer2d::drawRect(const Vector2f& pos, const Vector2f& size, const Vector3f& rotation, const Color& color,  float thickness) {
        if (thickness <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        const float h_thickness = std::min(thickness, size.y);
        const float v_thickness = std::min(thickness, size.x);

        const float left = pos.x;
        const float bottom = pos.y;
        const float right = pos.x + size.x;
        const float top = pos.y + size.y;

        QuadDrawContext q0{}, q1{}, q2{}, q3{};
        q0.texture_id = 0;
        q0.dst_rect = {left, bottom, size.x, h_thickness};
        q0.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        q0.rotation = rotation;
        q0.color = color.to_vec4();
        s_Data.submit.quads.push_back(q0);
        s_Data.dirty = true;
        if (h_thickness < size.y) {
            q1.texture_id = 0;
            q1.dst_rect = {left, top - h_thickness, size.x, h_thickness};
            q1.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
            q1.rotation = rotation;
            q1.color = color.to_vec4();
            s_Data.submit.quads.push_back(q1);
            s_Data.dirty = true;
        } 
        q2.texture_id = 0;
        q2.dst_rect = {left, bottom, v_thickness, size.y};
        q2.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        q2.rotation = rotation;
        q2.color = color.to_vec4();
        s_Data.submit.quads.push_back(q2);
        s_Data.dirty = true;
        if (v_thickness < size.x) { 
            q3.texture_id = 0;
            q3.dst_rect = {right - v_thickness, bottom, v_thickness, size.y};
            q3.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
            q3.rotation = rotation;
            q3.color = color.to_vec4();
            s_Data.submit.quads.push_back(q3);
            s_Data.dirty = true;
        }
    }

    void Renderer2d::drawLine(const Vector2f& start, const Vector2f& end, const Vector3f& rotation, const float thickness, const Color& color) {
        if (thickness <= 0.0f) return;

        LineDrawContext line{};
        line.start = start;
        line.end = end;
        line.rotation = rotation;
        line.color = color.to_vec4();
        line.thickness = thickness;

        s_Data.submit.lines.push_back(line);
        s_Data.dirty = true;
    }

    void Renderer2d::drawText() {

    }

    const std::vector<QuadCpuData>& Renderer2d::swapQuadCpuBatches() {
        if (!s_Data.dirty) {
            return s_Data.quad_batches;
        }

        buildQuadBatches(s_Data.quad_batches, s_Data.submit);
        s_Data.submit.clear();
        s_Data.dirty = false;
        return s_Data.quad_batches;
    }

    const QuadCpuData& Renderer2d::swapQuadCpuData() {
        static QuadCpuData empty{};
        const auto& batches = swapQuadCpuBatches();
        if (batches.empty()) {
            return empty;
        }
        return batches.front();
    }

    void Renderer2d::clearBatches() {
        s_Data.clear();
    }

} // dodoe
