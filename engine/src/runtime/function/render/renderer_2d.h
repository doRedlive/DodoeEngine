//
// Created by Redlive on 2026/3/17.
//

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"

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
        std::vector<identifier> textures{};

        void clear() {
            vertices.clear();
            indices.clear();
            textures.clear();
        }
    };

    struct QuadDrawContext {
        identifier texture_id{0};
        Vector4f dst_rect{0.0f};
        Vector4f uv_rect{0.0f};
        Vector3f rotation{0.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct LineDrarContext {
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

    class Renderer2d {
    public:
        static constexpr ui32 k_MaxQuadCount = 2048;
        static constexpr ui32 k_MaxTextureCount = 1024;

        static void drawSprite(
            identifier texture,
            const Vector2f& pos, 
            const Vector2f& size,
            const Vector3f& rotation, 
            const Color& color = Color::white()
        );

        static void drawRect(
            const Vector2f& pos, 
            const Vector2f& size,
            const Vector3f& rotation, 
            const Color& color = Color::white(), 
            float thickness = 1.0f
        );

        static void drawLine(
            const Vector2f& start, 
            const Vector2f& end, 
            const Vector3f& rotation,
            float thickness = 1.0f, 
            const Color& color = Color::white()
        );

        static void drawText();

        static const std::vector<QuadCpuData>& swapQuadCpuBatches();
        static const QuadCpuData& swapQuadCpuData();
        static void clearBatches();
    };

} // dodoe
