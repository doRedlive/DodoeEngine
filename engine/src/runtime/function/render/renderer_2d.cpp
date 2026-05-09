//
// Created by Redlive 2026/3/17.
//

#include "renderer_2d.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"
#include "framework/texture_manager.h"

namespace dodoe {

    namespace {
        TextureManager* GetTextureManager() {
            auto& app = Application::Self();
            auto* render_system = app.context().render_system.get();
            return render_system ? render_system->getTextureManager() : nullptr;
        }

        struct QuadDrawCommand {
            identifier texture_id{0};
            Vector4f dst_rect{0.0f};
            Vector4f uv_rect{0.0f};
            Vector3f rotation{0.0f};
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        };

        struct LineDrawCommand {
            Vector2f start{0.0f};
            Vector2f end{0.0f};
            Vector3f rotation{0.0f};
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
            float thickness{2.0f};
        };

        struct Renderer2dState {
            std::vector<QuadDrawCommand> quads{};
            std::vector<LineDrawCommand> lines{};
            std::vector<QuadCpuData> quad_batches{};
            bool dirty{false};

            void clear() {
                quads.clear();
                lines.clear();
                quad_batches.clear();
                dirty = false;
            }
        };

        static Renderer2dState s_Data;

        ui32 ResolveDescriptorIndex(identifier texture_id) {
            auto* texture_manager = GetTextureManager();
            if (!texture_manager) {
                return 0;
            }

            auto fallback = texture_manager->loadFallbackTexture();
            ui32 fallback_index = 0;
            if (fallback && fallback->descriptor_index >= 0) {
                fallback_index = static_cast<ui32>(fallback->descriptor_index);
            }

            if (texture_id == 0) {
                return fallback_index;
            }

            auto texture = texture_manager->loadTexture(texture_id);
            if (!texture || texture->descriptor_index < 0) {
                return fallback_index;
            }

            return static_cast<ui32>(texture->descriptor_index);
        }

        void AppendQuad(
            QuadCpuData& out,
            const Vector4f& dst_rect,
            const Vector4f& uv_rect,
            const Vector3f& rotation,
            const Vector4f& color,
            ui32 descriptor_index
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

            const float u0 = uv_rect.x;
            const float v0 = uv_rect.y;
            const float u1 = uv_rect.z;
            const float v1 = uv_rect.w;

            const ui32 base_vertex = static_cast<ui32>(out.vertices.size());
            out.vertices.push_back({
                Vector3f(cx - hx * cos_r + hy * sin_r, cy - hx * sin_r - hy * cos_r, 0.0f),
                {u0, v0},
                color,
                descriptor_index
            });
            out.vertices.push_back({
                Vector3f(cx + hx * cos_r + hy * sin_r, cy + hx * sin_r - hy * cos_r, 0.0f),
                {u1, v0},
                color,
                descriptor_index
            });
            out.vertices.push_back({
                Vector3f(cx + hx * cos_r - hy * sin_r, cy + hx * sin_r + hy * cos_r, 0.0f),
                {u1, v1},
                color,
                descriptor_index
            });
            out.vertices.push_back({
                Vector3f(cx - hx * cos_r - hy * sin_r, cy - hx * sin_r + hy * cos_r, 0.0f),
                {u0, v1},
                color,
                descriptor_index
            });

            out.indices.push_back(base_vertex + 0);
            out.indices.push_back(base_vertex + 1);
            out.indices.push_back(base_vertex + 2);
            out.indices.push_back(base_vertex + 2);
            out.indices.push_back(base_vertex + 3);
            out.indices.push_back(base_vertex + 0);
        }

        bool BuildLineRect(const LineDrawCommand& line, Vector4f& out_rect, Vector3f& out_rotation) {
            if (line.thickness <= 0.0f) {
                return false;
            }

            const Vector2f delta = line.end - line.start;
            const float length = Math::length(delta);
            const float half_thickness = line.thickness * 0.5f;

            out_rotation = line.rotation;
            if (length <= std::numeric_limits<float>::epsilon()) {
                out_rect = {
                    line.start.x - half_thickness,
                    line.start.y - half_thickness,
                    line.thickness,
                    line.thickness
                };
                return true;
            }

            const Vector2f center = (line.start + line.end) * 0.5f;
            out_rect = {
                center.x - length * 0.5f,
                center.y - half_thickness,
                length,
                line.thickness
            };
            out_rotation.z += Math::rad2deg(std::atan2(delta.y, delta.x));
            return true;
        }

        void FlushBatch(std::vector<QuadCpuData>& out, QuadCpuData& batch) {
            if (batch.vertices.empty() || batch.indices.empty()) {
                return;
            }
            out.push_back(std::move(batch));
            batch.clear();
        }

        void EnsureBatchRoomForOneQuad(std::vector<QuadCpuData>& out, QuadCpuData& batch) {
            const size_t quad_count = batch.vertices.size() / 4;
            if (quad_count >= static_cast<size_t>(Renderer2d::MaxQuadCount)) {
                FlushBatch(out, batch);
            }
        }

        void SubmitQuad(
            identifier texture_id,
            const Vector4f& dst_rect,
            const Vector4f& uv_rect,
            const Vector3f& rotation,
            const Vector4f& color
        ) {
            QuadDrawCommand quad{};
            quad.texture_id = texture_id;
            quad.dst_rect = dst_rect;
            quad.uv_rect = uv_rect;
            quad.rotation = rotation;
            quad.color = color;
            s_Data.quads.push_back(quad);
            s_Data.dirty = true;
        }

        void BuildQuadBatches(std::vector<QuadCpuData>& out) {
            out.clear();
            if (s_Data.quads.empty() && s_Data.lines.empty()) {
                return;
            }

            QuadCpuData batch{};
            const size_t submit_count = s_Data.quads.size() + s_Data.lines.size();
            const size_t reserve_quads = (std::min)(submit_count, static_cast<size_t>(Renderer2d::MaxQuadCount));
            batch.vertices.reserve(reserve_quads * 4);
            batch.indices.reserve(reserve_quads * 6);

            for (const auto& quad : s_Data.quads) {
                EnsureBatchRoomForOneQuad(out, batch);
                const ui32 descriptor_index = ResolveDescriptorIndex(quad.texture_id);
                AppendQuad(batch, quad.dst_rect, quad.uv_rect, quad.rotation, quad.color, descriptor_index);
            }

            const ui32 line_descriptor_index = ResolveDescriptorIndex(0);

            for (const auto& line : s_Data.lines) {
                Vector4f rect{};
                Vector3f line_rotation{0.0f};
                if (!BuildLineRect(line, rect, line_rotation)) {
                    continue;
                }

                EnsureBatchRoomForOneQuad(out, batch);
                AppendQuad(batch, rect, {0.0f, 0.0f, 1.0f, 1.0f}, line_rotation, line.color, line_descriptor_index);
            }

            FlushBatch(out, batch);
        }
    }

    void Renderer2d::drawSprite(const identifier texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Color& color) {
        SubmitQuad(texture, {pos.x, pos.y, size.x, size.y}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4());
    }

    void Renderer2d::drawRect(const Vector2f& pos, const Vector2f& size, const Vector3f& rotation, const Color& color,  float thickness) {
        if (thickness <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        const float h_thickness = (thickness < size.y) ? thickness : size.y;
        const float v_thickness = (thickness < size.x) ? thickness : size.x;

        const float left = pos.x;
        const float bottom = pos.y;
        const float right = pos.x + size.x;
        const float top = pos.y + size.y;
        const Vector4f uv{0.0f, 0.0f, 1.0f, 1.0f};
        const Vector4f color_v = color.to_vec4();

        SubmitQuad(0, {left, bottom, size.x, h_thickness}, uv, rotation, color_v);
        if (h_thickness < size.y) {
            SubmitQuad(0, {left, top - h_thickness, size.x, h_thickness}, uv, rotation, color_v);
        } 
        SubmitQuad(0, {left, bottom, v_thickness, size.y}, uv, rotation, color_v);
        if (v_thickness < size.x) { 
            SubmitQuad(0, {right - v_thickness, bottom, v_thickness, size.y}, uv, rotation, color_v);
        }
    }

    void Renderer2d::drawLine(const Vector2f& start, const Vector2f& end, const Vector3f& rotation, const float thickness, const Color& color) {
        if (thickness <= 0.0f) return;

        LineDrawCommand line{};
        line.start = start;
        line.end = end;
        line.rotation = rotation;
        line.color = color.to_vec4();
        line.thickness = thickness;

        s_Data.lines.push_back(line);
        s_Data.dirty = true;
    }

    void Renderer2d::drawText() {

    }

    const std::vector<QuadCpuData>& Renderer2d::swapQuadCpuBatches() {
        if (!s_Data.dirty) {
            return s_Data.quad_batches;
        }

        BuildQuadBatches(s_Data.quad_batches);
        s_Data.quads.clear();
        s_Data.lines.clear();
        s_Data.dirty = false;
        return s_Data.quad_batches;
    }

    void Renderer2d::clearBatches() {
        s_Data.clear();
    }

} // dodoe
