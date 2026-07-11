// do@Redlive

#include "debug_draw.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    DynamicArray<DebugDraw::Primitive> DebugDraw::s_primitives;

    void DebugDraw::Line(const Vector3f& a, const Vector3f& b, const Color& c)
    {
        s_primitives.push_back({
            PrimitiveType::Line,
            a, b,
            {c.r, c.g, c.b},
            0.0f, 0.0f, 0, Matrix4f(1.0f)
        });
    }

    void DebugDraw::Circle(const Vector3f& center, const Vector3f& normal,
                           float radius, const Color& c, int segments)
    {
        s_primitives.push_back({
            PrimitiveType::Circle,
            center, glm::normalize(normal),
            {c.r, c.g, c.b},
            radius, 0.0f, segments, Matrix4f(1.0f)
        });
    }

    void DebugDraw::Box(const Matrix4f& transform, const Color& c)
    {
        s_primitives.push_back({
            PrimitiveType::Box,
            {}, {},
            {c.r, c.g, c.b},
            0.0f, 0.0f, 0, transform
        });
    }

    void DebugDraw::Box(const Vector3f& min, const Vector3f& max, const Color& c)
    {
        Vector3f center = (min + max) * 0.5f;
        Vector3f extent = (max - min) * 0.5f;
        Matrix4f t = glm::translate(Matrix4f(1.0f), center) *
                     glm::scale(Matrix4f(1.0f), extent);
        Box(t, c);
    }

    void DebugDraw::Cone(const Vector3f& tip, const Vector3f& dir, float length,
                         float radius, const Color& c)
    {
        s_primitives.push_back({
            PrimitiveType::Cone,
            tip, glm::normalize(dir),
            {c.r, c.g, c.b},
            radius, length, 0, Matrix4f(1.0f)
        });
    }

    void DebugDraw::Sphere(const Vector3f& center, float radius, const Color& c, int segments)
    {
        s_primitives.push_back({
            PrimitiveType::Sphere,
            center, {},
            {c.r, c.g, c.b},
            radius, 0.0f, segments, Matrix4f(1.0f)
        });
    }

    void DebugDraw::ScreenSpaceQuad(const Vector2f& center, const Vector2f& size, const Color& c)
    {
        s_primitives.push_back({
            PrimitiveType::ScreenQuad,
            {center.x, center.y, 0.0f}, {size.x, size.y, 0.0f},
            {c.r, c.g, c.b},
            0.0f, 0.0f, 0, Matrix4f(1.0f)
        });
    }

    void DebugDraw::DrawGrid(const Vector3f& origin, float size, int divisions, const Color& c)
    {
        float half = size * 0.5f;
        float step = size / static_cast<float>(divisions);

        for (int i = 0; i <= divisions; ++i) {
            float offset = -half + static_cast<float>(i) * step;

            Line({origin.x - half, origin.y, origin.z + offset},
                 {origin.x + half, origin.y, origin.z + offset}, c);

            Line({origin.x + offset, origin.y, origin.z - half},
                 {origin.x + offset, origin.y, origin.z + half}, c);
        }
    }

    void DebugDraw::Flush()
    {
        // TODO: Submit primitives to render pipeline via DebugDrawRenderer
        Clear();
    }

    void DebugDraw::Clear()
    {
        s_primitives.clear();
    }

} // namespace dodoe
