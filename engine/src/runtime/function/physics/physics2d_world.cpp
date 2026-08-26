// do@Redlive

#include "physics2d_world.h"

#include <thread>

namespace dodoe {

    namespace {

        UInt32 ToRgba32(const b2HexColor color) {
            return (static_cast<UInt32>(color) << 8) | 0xFF;
        }

    }

    void Physics2dWorld::step(const float dt) {
        m_accumulator += dt;
        m_last_step_count = 0;
        while (m_accumulator >= m_fixed_dt && m_last_step_count < m_max_sub_steps) {
            b2World_Step(m_world_id, m_fixed_dt, m_sub_step_count);
            m_accumulator -= m_fixed_dt;
            ++m_last_step_count;
        }
        if (m_last_step_count == m_max_sub_steps && m_accumulator >= m_fixed_dt) {
            m_accumulator = std::fmod(m_accumulator, m_fixed_dt);
        }
        collectContactEvents();
        b2World_Draw(m_world_id, &m_debug_draw);
        m_debugger->flush();
    }

    void Physics2dWorld::collectContactEvents() {
        const b2ContactEvents events = b2World_GetContactEvents(m_world_id);

        for (int i = 0; i < events.beginCount; ++i) {
            const b2ContactBeginTouchEvent& begin = events.beginEvents[i];
            Contact2dEvent event;
            event.shape_a = begin.shapeIdA;
            event.shape_b = begin.shapeIdB;
            if (begin.manifold.pointCount > 0) {
                event.point = begin.manifold.points[0].point;
                event.normal = begin.manifold.normal;
            }
            event.phase = Contact2dPhase::Begin;
            m_contact_events.push_back(event);
        }

        for (int i = 0; i < events.endCount; ++i) {
            const b2ContactEndTouchEvent& end = events.endEvents[i];
            m_contact_events.push_back({ end.shapeIdA, end.shapeIdB, {}, {}, 0.0f, Contact2dPhase::End });
        }

        for (int i = 0; i < events.hitCount; ++i) {
            const b2ContactHitEvent& hit = events.hitEvents[i];
            m_contact_events.push_back({ hit.shapeIdA, hit.shapeIdB, hit.point, hit.normal, hit.approachSpeed, Contact2dPhase::Hit });
        }
    }

    void Physics2dWorld::takeContactEvents(DynamicArray<Contact2dEvent>& out_events) {
        out_events.clear();
        if (m_contact_events.empty()) {
            return;
        }
        out_events.swap(m_contact_events);
    }

    void Physics2dWorld::raycast(const Vector2f& origin, const Vector2f& direction, const float max_distance,
                                 const Query2dFilter& filter, DynamicArray<Raycast2dHit>& out_hits) const {
        out_hits.clear();

        b2QueryFilter query_filter = b2DefaultQueryFilter();
        query_filter.categoryBits = filter.layer;
        query_filter.maskBits = filter.mask;

        const b2Vec2 translation{ direction.x * max_distance, direction.y * max_distance };
        b2World_CastRay(m_world_id, { origin.x, origin.y }, translation, query_filter, &Physics2dWorld::OnRayCast, &out_hits);

        std::sort(out_hits.begin(), out_hits.end(), [](const Raycast2dHit& a, const Raycast2dHit& b) {
            return a.fraction < b.fraction;
        });
    }

    void Physics2dWorld::overlapAABB(const Vector2f& center, const Vector2f& half_size,
                                     const Query2dFilter& filter, DynamicArray<b2ShapeId>& out_shapes) const {
        out_shapes.clear();

        b2QueryFilter query_filter = b2DefaultQueryFilter();
        query_filter.categoryBits = filter.layer;
        query_filter.maskBits = filter.mask;

        const b2AABB aabb{
            { center.x - half_size.x, center.y - half_size.y },
            { center.x + half_size.x, center.y + half_size.y }
        };
        b2World_OverlapAABB(m_world_id, aabb, query_filter, &Physics2dWorld::OnOverlap, &out_shapes);
    }

    float Physics2dWorld::OnRayCast(const b2ShapeId shape_id, const b2Vec2 point, const b2Vec2 normal, const float fraction, void* context) {
        auto* hits = static_cast<DynamicArray<Raycast2dHit>*>(context);
        hits->push_back({ shape_id, { point.x, point.y }, { normal.x, normal.y }, fraction });
        return 1.0f;
    }

    bool Physics2dWorld::OnOverlap(const b2ShapeId shape_id, void* context) {
        auto* shapes = static_cast<DynamicArray<b2ShapeId>*>(context);
        shapes->push_back(shape_id);
        return true;
    }

    bool Physics2dWorld::initialize(const Physics2dWorldCreateInfo& create_info) {
        m_world_id = b2WorldId();

        if (B2_IS_NON_NULL(m_world_id)) {
            DO_ERROR("Can't create box2d world");
            return false;
        }

        b2WorldDef world_def = b2DefaultWorldDef();
        world_def.gravity.x = 0.0f;
        world_def.gravity.y = create_info.gravity;
        world_def.workerCount = static_cast<i32>(
            std::max(1u, std::thread::hardware_concurrency() - 2));
        world_def.enableSleep = true;
        m_world_id = b2CreateWorld(&world_def);

        m_debugger = PhysicsDebugger::Create({});

        m_debug_draw = b2DefaultDebugDraw();
        m_debug_draw.DrawPolygonFcn = &Physics2dWorld::OnDrawPolygon;
        m_debug_draw.DrawSolidPolygonFcn = &Physics2dWorld::OnDrawSolidPolygon;
        m_debug_draw.DrawCircleFcn = &Physics2dWorld::OnDrawCircle;
        m_debug_draw.DrawSolidCircleFcn = &Physics2dWorld::OnDrawSolidCircle;
        m_debug_draw.DrawSolidCapsuleFcn = &Physics2dWorld::OnDrawSolidCapsule;
        m_debug_draw.DrawSegmentFcn = &Physics2dWorld::OnDrawSegment;
        m_debug_draw.DrawTransformFcn = &Physics2dWorld::OnDrawTransform;
        m_debug_draw.DrawPointFcn = &Physics2dWorld::OnDrawPoint;
        m_debug_draw.DrawStringFcn = &Physics2dWorld::OnDrawString;
        m_debug_draw.drawShapes = true;
        m_debug_draw.drawJoints = true;
        m_debug_draw.drawJointExtras = true;
        m_debug_draw.context = this;

        m_sub_step_count = create_info.sub_step_count;
        m_fixed_dt = create_info.fixed_dt;
        m_max_sub_steps = create_info.max_sub_steps;
        return m_debugger != nullptr;
    }

    void Physics2dWorld::shutdown() {
        PhysicsDebugger::Destroy(m_debugger);

        if (B2_IS_NON_NULL(m_world_id)) {
            b2DestroyWorld(m_world_id);
        }

        m_world_id = b2_nullWorldId;
        m_debug_draw = b2DefaultDebugDraw();
    }

    void Physics2dWorld::drawLine(const b2Vec2& start, const b2Vec2& end, const b2HexColor color) {
        if (!m_debugger) {
            return;
        }
        m_debugger->drawLine(
            Vector3f(start.x, start.y, 0.0f),
            Vector3f(end.x, end.y, 0.0f),
            ToRgba32(color));
    }

    void Physics2dWorld::OnDrawPolygon(const b2Vec2* vertices, const int vertex_count, const b2HexColor color, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        if (!vertices || vertex_count < 2) {
            return;
        }
        for (int i = 0; i < vertex_count; ++i) {
            world->drawLine(vertices[i], vertices[(i + 1) % vertex_count], color);
        }
    }

    void Physics2dWorld::OnDrawSolidPolygon(const b2Transform transform, const b2Vec2* vertices, const int vertex_count, const float radius, const b2HexColor color, void* context) {
        (void)radius;
        auto* world = static_cast<Physics2dWorld*>(context);
        if (!vertices || vertex_count < 2) {
            return;
        }

        DynamicArray<b2Vec2> transformed_vertices{};
        transformed_vertices.reserve(static_cast<size_t>(vertex_count));
        for (int i = 0; i < vertex_count; ++i) {
            transformed_vertices.push_back(b2TransformPoint(transform, vertices[i]));
        }
        for (int i = 0; i < vertex_count; ++i) {
            world->drawLine(transformed_vertices[i], transformed_vertices[(i + 1) % vertex_count], color);
        }
    }

    void Physics2dWorld::OnDrawCircle(const b2Vec2 center, const float radius, const b2HexColor color, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        if (radius <= 0.0f) {
            return;
        }

        constexpr int segment_count = 24;
        constexpr float two_pi = 6.28318530717958647692f;
        for (int i = 0; i < segment_count; ++i) {
            const float t0 = two_pi * static_cast<float>(i) / static_cast<float>(segment_count);
            const float t1 = two_pi * static_cast<float>(i + 1) / static_cast<float>(segment_count);
            const b2Vec2 p0{ center.x + std::cos(t0) * radius, center.y + std::sin(t0) * radius };
            const b2Vec2 p1{ center.x + std::cos(t1) * radius, center.y + std::sin(t1) * radius };
            world->drawLine(p0, p1, color);
        }
    }

    void Physics2dWorld::OnDrawSolidCircle(const b2Transform transform, const float radius, const b2HexColor color, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        OnDrawCircle(transform.p, radius, color, context);

        const b2Vec2 axis_end = b2TransformPoint(transform, { radius, 0.0f });
        world->drawLine(transform.p, axis_end, color);
    }

    void Physics2dWorld::OnDrawSolidCapsule(const b2Vec2 p1, const b2Vec2 p2, const float radius, const b2HexColor color, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        world->drawLine(p1, p2, color);
        OnDrawCircle(p1, radius, color, context);
        OnDrawCircle(p2, radius, color, context);
    }

    void Physics2dWorld::OnDrawSegment(const b2Vec2 p1, const b2Vec2 p2, const b2HexColor color, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        world->drawLine(p1, p2, color);
    }

    void Physics2dWorld::OnDrawTransform(const b2Transform transform, void* context) {
        auto* world = static_cast<Physics2dWorld*>(context);
        const b2Vec2 x_axis = b2TransformPoint(transform, { 0.5f, 0.0f });
        const b2Vec2 y_axis = b2TransformPoint(transform, { 0.0f, 0.5f });
        world->drawLine(transform.p, x_axis, b2_colorRed);
        world->drawLine(transform.p, y_axis, b2_colorGreen);
    }

    void Physics2dWorld::OnDrawPoint(const b2Vec2 p, const float size, const b2HexColor color, void* context) {
        (void)size;
        auto* world = static_cast<Physics2dWorld*>(context);
        if (!world->m_debugger) {
            return;
        }
        world->m_debugger->drawPoint(Vector3f(p.x, p.y, 0.0f), ToRgba32(color));
    }

    void Physics2dWorld::OnDrawString(const b2Vec2 p, const char* s, const b2HexColor color, void* context) {
        (void)p;
        (void)s;
        (void)color;
        (void)context;
    }

} // dodoe
