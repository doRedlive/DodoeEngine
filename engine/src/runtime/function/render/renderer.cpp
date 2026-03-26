//
// Created by Redlive 2026/3/17.
//

#include "renderer.h"

#include "render_resource.h"

#include "runtime/core/math/math.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    void Renderer::draw_sprite(const Ref<Texture>& texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Color& color, RenderStageType stage) {
        QuadDrawContext quad;
        if (!texture) {
            DoError("Renderer::draw_sprite: texture is null.");
            quad = QuadDrawContext(nullptr, {pos.x, pos.y, size.x, size.y}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4());
            g_render_resource->submit(quad);
            return;
        }
        
        quad = QuadDrawContext(texture, {pos.x, pos.y, size.x, size.y}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4());
        g_render_resource->submit(quad);
    }

    void Renderer::draw_rect(
        const Vector2f& pos, 
        const Vector2f& size, 
        const Vector3f& rotation,
        const Color& color, 
        float thickness, 
        RenderStageType stage
    ) {
        if (thickness <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        const float h_thickness = std::min(thickness, size.y);
        const float v_thickness = std::min(thickness, size.x);

        const float left = pos.x;
        const float bottom = pos.y;
        const float right = pos.x + size.x;
        const float top = pos.y + size.y;

        QuadDrawContext q0, q1, q2, q3;
        q0 = QuadDrawContext(nullptr, {left, bottom, size.x, h_thickness}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4(), stage);
        g_render_resource->submit(q0);
        if (h_thickness < size.y) {
            q1 = QuadDrawContext(nullptr, {left, top - h_thickness, size.x, h_thickness}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4(), stage);
            g_render_resource->submit(q1);
        } 
        q2 = QuadDrawContext(nullptr, {left, bottom, v_thickness, size.y}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4(), stage);
        g_render_resource->submit(q2);
        if (v_thickness < size.x) { 
            q3 = QuadDrawContext(nullptr, {right - v_thickness, bottom, v_thickness, size.y}, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4(), stage);
            g_render_resource->submit(q3);
        }
    }

    void Renderer::draw_line(const Vector2f& start, const Vector2f& end, const Vector3f& rotation, const float thickness, const Color& color, RenderStageType stage) {
        if (thickness <= 0.0f) return;

        const Vector2f delta = end - start;
        const float length = Math::length(delta);
        const float half_thickness = thickness * 0.5f;

        const Vector2f min_point{std::min(start.x, end.x), std::min(start.y, end.y)};
        const Vector2f max_point{std::max(start.x, end.x), std::max(start.y, end.y)};
        const Rect cull_rect{
            {min_point.x - half_thickness, min_point.y - half_thickness},
            {std::max(max_point.x - min_point.x, 0.0f) + thickness,
             std::max(max_point.y - min_point.y, 0.0f) + thickness}
        };

        // if (should_cull_rect(cull_rect)) return;

        QuadDrawContext quad;
        if (length <= std::numeric_limits<float>::epsilon()) {
            const Vector4f rect {
                start.x - half_thickness,
                start.y - half_thickness,
                thickness,
                thickness
            };
            quad = QuadDrawContext(nullptr, rect, {0.0f, 0.0f, 1.0f, 1.0f}, rotation, color.to_vec4(), stage);
            g_render_resource->submit(quad);
            return;
        }

        Vector2f center = (start + end) * 0.5f;
        const Vector4f rect{
            center.x - length * 0.5f,
            center.y - half_thickness,
            length,
            thickness
        };
        const float angle = Math::rad2deg(std::atan2(delta.y, delta.x)) + rotation.z;

        quad = QuadDrawContext(nullptr, rect, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, angle}, color.to_vec4(), stage);
        g_render_resource->submit(quad);
    }

    void Renderer::draw_text() {

    }

} // dodoe
