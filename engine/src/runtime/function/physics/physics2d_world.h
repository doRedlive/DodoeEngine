// do@Redlive

#pragma once

#include "dopch.h"

#include "physics_debug.h"
#include "box2d/box2d.h"
#include "box2d/types.h"

namespace dodoe {

    struct Physics2dWorldCreateInfo {
        float gravity{-9.8f};
        int sub_step_count{4};
        float fixed_dt{1.0f / 60.0f};
        int max_sub_steps{4};
    };

    enum class Contact2dPhase {
        Begin = 0,
        End = 1,
        Hit = 2
    };

    struct Contact2dEvent {
        b2ShapeId shape_a{};
        b2ShapeId shape_b{};
        b2Vec2 point{};
        b2Vec2 normal{};
        float relative_speed{0.0f};
        Contact2dPhase phase{Contact2dPhase::Begin};
    };

    struct Query2dFilter {
        ui32 layer{1};
        ui32 mask{0xFFFFFFFF};
    };

    struct Raycast2dHit {
        b2ShapeId shape{};
        Vector2f point{};
        Vector2f normal{};
        float fraction{0.0f};
    };

    class Physics2dWorld : public Managed<Physics2dWorld, Physics2dWorldCreateInfo> {
        friend class Managed<Physics2dWorld, Physics2dWorldCreateInfo>;
    public:
        void step(float dt);
        [[nodiscard]] b2WorldId getWorldId() const { return m_world_id; }
        [[nodiscard]] float getFixedDt() const { return m_fixed_dt; }
        [[nodiscard]] int getLastStepCount() const { return m_last_step_count; }
        void takeContactEvents(DynamicArray<Contact2dEvent>& out_events);
        void raycast(const Vector2f& origin, const Vector2f& direction, float max_distance,
                     const Query2dFilter& filter, DynamicArray<Raycast2dHit>& out_hits) const;
        void overlapAABB(const Vector2f& center, const Vector2f& half_size,
                         const Query2dFilter& filter, DynamicArray<b2ShapeId>& out_shapes) const;

    private:
        b2WorldId m_world_id{};
        int m_sub_step_count{0};
        float m_fixed_dt{1.0f / 60.0f};
        int m_max_sub_steps{4};
        float m_accumulator{0.0f};
        int m_last_step_count{0};
        Scope<PhysicsDebugger> m_debugger{nullptr};
        b2DebugDraw m_debug_draw{};
        DynamicArray<Contact2dEvent> m_contact_events{};

        bool initialize(const Physics2dWorldCreateInfo& create_info);
        void shutdown();

        void drawLine(const b2Vec2& start, const b2Vec2& end, b2HexColor color);
        void collectContactEvents();

        static float OnRayCast(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* context);
        static bool OnOverlap(b2ShapeId shape_id, void* context);

        static void OnDrawPolygon(const b2Vec2* vertices, int vertex_count, b2HexColor color, void* context);
        static void OnDrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertex_count, float radius, b2HexColor color, void* context);
        static void OnDrawCircle(b2Vec2 center, float radius, b2HexColor color, void* context);
        static void OnDrawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context);
        static void OnDrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context);
        static void OnDrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context);
        static void OnDrawTransform(b2Transform transform, void* context);
        static void OnDrawPoint(b2Vec2 p, float size, b2HexColor color, void* context);
        static void OnDrawString(b2Vec2 p, const char* s, b2HexColor color, void* context);
    };

} // dodoe
