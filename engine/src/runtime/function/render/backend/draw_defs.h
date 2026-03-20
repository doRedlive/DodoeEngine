//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_DRAW_DEFS_H
#define DODOE_DRAW_DEFS_H

#include "dopch.h"
#include "runtime/function/render/backend/texture.h"

namespace dodoe {
    struct TextureDrawContext {
        Ref<Texture> texture;
        Vector4f dst_rect{};
        Vector4f uv_rect{};
        Vector3f rotation{};
        Vector4f color{}; 
    };
} // dodoe

#endif//DODOE_DRAW_DEFS_H