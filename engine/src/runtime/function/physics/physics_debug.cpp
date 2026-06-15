//
// Created by Redlive on 2026/3/26.
//

#include "physics_debug.h"

#include "runtime/function/render/renderer.h"

namespace dodoe {

    namespace {

        void draw_debug_line(const b2Vec2 p0, const b2Vec2 p1, const b2HexColor color, void* context) {
            (void)context; (void)color; (void)p0; (void)p1;
        }

        void DrawPolygon(const b2Vec2* vertices, int vertex_count, b2HexColor color, void* context) {
            if (!vertices || vertex_count < 2) {
                return;
            }

            for (int i = 0; i < vertex_count; ++i) {
                const b2Vec2 p0 = vertices[i];
                const b2Vec2 p1 = vertices[(i + 1) % vertex_count];
                draw_debug_line(p0, p1, color, context);
            }

        }

        void DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertex_count, float radius, b2HexColor color, void* context) {
            (void)radius;
            if (!vertices || vertex_count < 2) {
                return;
            }

            std::vector<b2Vec2> transformed_vertices(static_cast<size_t>(vertex_count));
            for (int i = 0; i < vertex_count; ++i) {
                transformed_vertices[static_cast<size_t>(i)] = b2TransformPoint(transform, vertices[i]);
            }

            DrawPolygon(transformed_vertices.data(), vertex_count, color, context);
        }

        void DrawCircle(b2Vec2 center, float radius, b2HexColor color, void* context) {
            if (radius <= 0.0f) {
                return;
            }

            constexpr int segment_count = 24;
            constexpr float two_pi = 6.28318530717958647692f;
            for (int i = 0; i < segment_count; ++i) {
                const float t0 = two_pi * static_cast<float>(i) / static_cast<float>(segment_count);
                const float t1 = two_pi * static_cast<float>(i + 1) / static_cast<float>(segment_count);
                const b2Vec2 p0{center.x + std::cos(t0) * radius, center.y + std::sin(t0) * radius};
                const b2Vec2 p1{center.x + std::cos(t1) * radius, center.y + std::sin(t1) * radius};
                draw_debug_line(p0, p1, color, context);
            }
        }

        void DrawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context) {
            DrawCircle(transform.p, radius, color, context);
            auto* draw_context = static_cast<DebugDrawContext*>(context);
            const float axis_len = std::max(draw_context ? draw_context->axis_length : 0.5f, radius);
            const b2Vec2 axis_end = b2TransformPoint(transform, {axis_len, 0.0f});
            draw_debug_line(transform.p, axis_end, color, context);
        }

        void DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context) {
            draw_debug_line(p1, p2, color, context);
            DrawCircle(p1, radius, color, context);
            DrawCircle(p2, radius, color, context);
        }

        void DrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context) {
            draw_debug_line(p1, p2, color, context);
        }

        void DrawTransform(b2Transform transform, void* context) {
            auto* draw_context = static_cast<DebugDrawContext*>(context);
            const float len = draw_context ? draw_context->axis_length : 0.5f;
            const b2Vec2 origin = transform.p;
            const b2Vec2 x_axis = b2TransformPoint(transform, {len, 0.0f});
            const b2Vec2 y_axis = b2TransformPoint(transform, {0.0f, len});
            draw_debug_line(origin, x_axis, b2_colorRed, context);
            draw_debug_line(origin, y_axis, b2_colorGreen, context);
        }

        void DrawPoint(b2Vec2 p, float size, b2HexColor color, void* context) {
            auto* draw_context = static_cast<DebugDrawContext*>(context);
            const float point_size = draw_context ? draw_context->point_size : 4.0f;
            const float draw_size = std::max(size, point_size);
            const float half = draw_size * 0.5f;
            (void)p; (void)size; (void)color;
        }

        void DrawString(b2Vec2 p, const char* s, b2HexColor color, void* context) {
            (void)p;
            (void)s;
            (void)color;
            (void)context;
        }

    }

    bool PhysicsDebugger::initialize(const PhysicsDebuggerCreateInfo& create_info) {
        debug_draw_context_.line_thickness = std::max(create_info.line_thickness, 0.1f);
        debug_draw_context_.point_size = std::max(create_info.point_size, 0.1f);
        debug_draw_context_.axis_length = std::max(create_info.axis_length, 0.1f);

        debug_draw_ = b2DefaultDebugDraw();
        debug_draw_.DrawPolygonFcn = DrawPolygon;
        debug_draw_.DrawSolidPolygonFcn = DrawSolidPolygon;
        debug_draw_.DrawCircleFcn = DrawCircle;
        debug_draw_.DrawSolidCircleFcn = DrawSolidCircle;
        debug_draw_.DrawSolidCapsuleFcn = DrawSolidCapsule;
        debug_draw_.DrawSegmentFcn = DrawSegment;
        debug_draw_.DrawTransformFcn = DrawTransform;
        debug_draw_.DrawPointFcn = DrawPoint;
        debug_draw_.DrawStringFcn = DrawString;

        debug_draw_.drawShapes = true;
        debug_draw_.drawJoints = true;
        debug_draw_.drawJointExtras = true;
        debug_draw_.drawBounds = false;
        debug_draw_.drawMass = false;
        debug_draw_.drawBodyNames = false;
        debug_draw_.drawContacts = false;
        debug_draw_.drawGraphColors = false;
        debug_draw_.drawContactNormals = false;
        debug_draw_.drawContactImpulses = false;
        debug_draw_.drawContactFeatures = false;
        debug_draw_.drawFrictionImpulses = false;
        debug_draw_.drawIslands = false;
        debug_draw_.context = &debug_draw_context_;
        return true;
    }

    void PhysicsDebugger::shutdown() {
        debug_draw_ = b2DefaultDebugDraw();
        debug_draw_context_ = DebugDrawContext{};
    }

} // dodoe
