//
// Created by Redlive on 2026/3/17.
//

#include "vk_renderer.h"

namespace dodoe {

    void VkRenderer::draw_sprite(const Ref<Texture>& texture, const Vector2f& pos,
        const Vector2f& size, const Vector3f& rotation, const Vector4f& color) {
        (void)texture;
        (void)pos;
        (void)size;
        (void)rotation;
        (void)color;
    }

    void VkRenderer::initialize(RendererCreateInfo create_info) {
        (void)create_info;

    }

    void VkRenderer::shutdown() {

    }

} // dodoe