// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"

#ifdef DrawText
#undef DrawText
#endif

namespace dodoe {

    struct QuadVertex {
        Vector3f position{0.0f, 0.0f, 0.0f};
        Vector2f uv{0.0f, 0.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        ui32 texture_index{0};
    };

    struct QuadCpuData {
        std::vector<QuadVertex> vertices{};
        std::vector<ui32> indices{};

        void clear() {
            vertices.clear();
            indices.clear();
        }
    };

    class Renderer2D {
    public:
        static constexpr UInt32 kMaxQuadCount = 2048;

        static void DrawSprite(
            Identifier texture,
            const Vector2f& pos,
            const Vector2f& size,
            const Vector3f& rotation,
            const Color& color = Color::white()
        );

        static void DrawSprite(
            Identifier texture,
            const Vector2f& pos,
            const Vector2f& size,
            const Vector3f& rotation,
            const Vector4f& uv,
            const Color& color
        );

        static void DrawRect(
            const Vector2f& pos,
            const Vector2f& size,
            const Vector3f& rotation,
            const Color& color = Color::white(),
            float thickness = 1.0f
        );

        static void DrawLine(
            const Vector2f& start,
            const Vector2f& end,
            const Vector3f& rotation,
            float thickness = 1.0f,
            const Color& color = Color::white()
        );

        static void DrawText();

        static const std::vector<QuadCpuData>& GetQuadCpuBatches();
        static void ClearBatches();
    };

} // dodoe
