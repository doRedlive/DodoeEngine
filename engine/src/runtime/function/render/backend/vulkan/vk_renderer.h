//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_VK_RENDERER_H
#define DODOE_VK_RENDERER_H

#include "dopch.h"

#include "runtime/function/render/renderer.h"

namespace dodoe {

    class VkRenderer : public Renderer {
    public:
        void draw_sprite(const Ref<Texture>& texture, const Vector2f& pos,
            const Vector2f& size, const Vector3f& rotation, const Vector4f& color);

    protected:
        void initialize(RendererCreateInfo create_info);
        void shutdown();
    };
    
} // dodoe

#endif//DODOE_VK_RENDERER_H