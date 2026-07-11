// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    class DODOE_API DebugDraw {
    public:
        // ---- Primitive submission ----
        static void Line(const Vector3f& a, const Vector3f& b, const Color& c);
        static void Circle(const Vector3f& center, const Vector3f& normal, float radius, const Color& c, int segments = 64);
        static void Box(const Matrix4f& transform, const Color& c);
        static void Box(const Vector3f& min, const Vector3f& max, const Color& c);
        static void Cone(const Vector3f& tip, const Vector3f& dir, float length, float radius, const Color& c);
        static void Sphere(const Vector3f& center, float radius, const Color& c, int segments = 16);
        static void ScreenSpaceQuad(const Vector2f& center, const Vector2f& size, const Color& c);

        // Grid helpers
        static void DrawGrid(const Vector3f& origin, float size, int divisions, const Color& c);

        // ---- Lifecycle ----
        static void Flush();
        static void Clear();

    private:
        friend class DebugDrawRenderer;

        enum class PrimitiveType : UInt8 {
            Line,
            Circle,
            Box,
            Cone,
            Sphere,
            ScreenQuad,
        };

        struct Primitive {
            PrimitiveType type;
            Vector3f a, b;
            Vector3f color_vec;
            float radius;
            float length;
            int   segments;
            Matrix4f transform;
        };

        static DynamicArray<Primitive> s_primitives;
    };

} // namespace dodoe
