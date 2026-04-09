//
// Created by Redlive 2026/3/17.
//

#include "renderer_2d.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    namespace {
        struct Render2dCpuData {
            std::unordered_map<identifier, ui32> texture_index_umap{};
            std::vector<QuadDrawData> quad_data{};
            std::vector<LineDrawData> line_data{};
            std::vector<TextDrawData> text_data{};
            RenderCpu2dData batched_cpu_data{};
            bool dirty{false};

            static constexpr float kPi = 3.1415926535f;

            void clearBatches() {
                quad_data.clear();
                line_data.clear();
                text_data.clear();
                dirty = true;
            }

            void clearBatchingCache() {
                texture_index_umap.clear();
            }

            ui32 resolveTextureSlot(identifier texture_id, RenderCpu2dData& out) {
                auto slot_it = texture_index_umap.find(texture_id);
                if (slot_it != texture_index_umap.end()) {
                    return slot_it->second;
                }

                const ui32 texture_index = static_cast<ui32>(out.textures.size());
                texture_index_umap.emplace(texture_id, texture_index);
                out.textures.push_back(texture_id);
                return texture_index;
            }

            void pushQuad(
                const Vector4f& dst_rect,
                const Vector4f& uv_rect,
                const Vector3f& rotation,
                const Vector4f& color,
                const ui32 texture_index,
                RenderCpu2dData& out
            ) {
                const float x = dst_rect.x;
                const float y = dst_rect.y;
                const float w = dst_rect.z;
                const float h = dst_rect.w;

                const float cx = x + w * 0.5f;
                const float cy = y + h * 0.5f;
                const float hx = w * 0.5f;
                const float hy = h * 0.5f;

                const float radians = rotation.z * (kPi / 180.0f);
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

            void batchQuads(const std::vector<QuadDrawData>& quads, RenderCpu2dData& out) {
                out.vertices.reserve(quads.size() * 4);
                out.indices.reserve(quads.size() * 6);
                out.textures.reserve(quads.size());

                texture_index_umap.reserve(quads.size());

                for (const auto& quad : quads) {
                    const ui32 texture_index = resolveTextureSlot(quad.texture_id, out);
                    pushQuad(quad.dst_rect, quad.uv_rect, quad.rotation, quad.color, texture_index, out);
                }
            }

            void batchLines(const std::vector<LineDrawData>& lines, RenderCpu2dData& out) {
                if (lines.empty()) {
                    return;
                }

                const ui32 white_slot = resolveTextureSlot(0, out);

                for (const auto& line : lines) {
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

                    pushQuad(rect, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, line.color, white_slot, out);
                }
            }

            void batchTexts(const std::vector<TextDrawData>& texts, RenderCpu2dData& out) {
                for (const auto& text : texts) {
                    const ui32 texture_index = resolveTextureSlot(text.texture_id, out);
                    pushQuad(text.dst_rect, text.uv_rect, text.rotation, text.color, texture_index, out);
                }
            }

            const RenderCpu2dData& gainCpuBatchData() {
                if (!dirty) {
                    return batched_cpu_data;
                }

                clearBatchingCache();
                batched_cpu_data.clear();

                batchQuads(quad_data, batched_cpu_data);
                batchLines(line_data, batched_cpu_data);
                batchTexts(text_data, batched_cpu_data);

                dirty = false;
                return batched_cpu_data;
            }
        };

        static Render2dCpuData s_Render2dCpuData;
    }

    void Renderer2d::drawSprite(const identifier texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Color& color) {
        if (!texture) {
            DoError("Renderer2d::drawSprite: texture is null.");
            return;
        }

        QuadDrawData quad{};
        quad.texture_id = texture;
        quad.dst_rect = {pos.x, pos.y, size.x, size.y};
        quad.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        quad.rotation = rotation;
        quad.color = color.to_vec4();

        s_Render2dCpuData.quad_data.push_back(quad);
        s_Render2dCpuData.dirty = true;
    }

    void Renderer2d::drawRect(
        const Vector2f& pos, 
        const Vector2f& size, 
        const Vector3f& rotation,
        const Color& color, 
        float thickness
    ) {
        if (thickness <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        const float h_thickness = std::min(thickness, size.y);
        const float v_thickness = std::min(thickness, size.x);

        const float left = pos.x;
        const float bottom = pos.y;
        const float right = pos.x + size.x;
        const float top = pos.y + size.y;

        QuadDrawData q0{}, q1{}, q2{}, q3{};
        q0.texture_id = 0;
        q0.dst_rect = {left, bottom, size.x, h_thickness};
        q0.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        q0.rotation = rotation;
        q0.color = color.to_vec4();
        s_Render2dCpuData.quad_data.push_back(q0);
        s_Render2dCpuData.dirty = true;
        if (h_thickness < size.y) {
            q1.texture_id = 0;
            q1.dst_rect = {left, top - h_thickness, size.x, h_thickness};
            q1.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
            q1.rotation = rotation;
            q1.color = color.to_vec4();
            s_Render2dCpuData.quad_data.push_back(q1);
            s_Render2dCpuData.dirty = true;
        } 
        q2.texture_id = 0;
        q2.dst_rect = {left, bottom, v_thickness, size.y};
        q2.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
        q2.rotation = rotation;
        q2.color = color.to_vec4();
        s_Render2dCpuData.quad_data.push_back(q2);
        s_Render2dCpuData.dirty = true;
        if (v_thickness < size.x) { 
            q3.texture_id = 0;
            q3.dst_rect = {right - v_thickness, bottom, v_thickness, size.y};
            q3.uv_rect = {0.0f, 0.0f, 1.0f, 1.0f};
            q3.rotation = rotation;
            q3.color = color.to_vec4();
            s_Render2dCpuData.quad_data.push_back(q3);
            s_Render2dCpuData.dirty = true;
        }
    }

    void Renderer2d::drawLine(const Vector2f& start, const Vector2f& end, const Vector3f& rotation, const float thickness, const Color& color) {
        if (thickness <= 0.0f) return;

        LineDrawData line{};
        line.start = start;
        line.end = end;
        line.rotation = rotation;
        line.color = color.to_vec4();
        line.thickness = thickness;

        s_Render2dCpuData.line_data.push_back(line);
        s_Render2dCpuData.dirty = true;
    }

    void Renderer2d::drawText() {

    }

    const RenderCpu2dData& Renderer2d::gainRenderCpuData() {
        return s_Render2dCpuData.gainCpuBatchData();
    }

    void Renderer2d::clearBatches() {
        s_Render2dCpuData.clearBatches();
    }

} // dodoe
