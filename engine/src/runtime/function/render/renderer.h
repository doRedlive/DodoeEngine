//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDERER_H
#define DODOE_RENDERER_H

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "runtime/function/render/backend/texture.h"

namespace dodoe {

    enum class RenderStageType {
        Sprite = 0,
        Debug,
        Ui,
    };

    class Renderer {
    public:
        static void draw_sprite(
            const Ref<Texture>& texture,
            const Vector2f& pos, 
            const Vector2f& size,
            const Vector3f& rotation, 
            const Color& color = Color::white(), 
            RenderStageType stage = RenderStageType::Sprite
        );

        static void draw_rect(
            const Vector2f& pos, 
            const Vector2f& size,
            const Vector3f& rotation, 
            const Color& color = Color::white(), 
            float thickness = 1.0f, 
            RenderStageType stage = RenderStageType::Sprite
        );

        static void draw_line(
            const Vector2f& start, 
            const Vector2f& end, 
            const Vector3f& rotation,
            float thickness = 1.0f, 
            const Color& color = Color::white(), 
            RenderStageType stage = RenderStageType::Sprite
        );

        static void draw_text();
    };

} // dodoe

#endif//DODOE_RENDERER_H
