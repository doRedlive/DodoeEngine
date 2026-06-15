// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    struct alignas(16) SpriteInstance {
        Float position_x{0.0f};
        Float position_y{0.0f};
        Float scale_x{1.0f};
        Float scale_y{1.0f};
        Float rotation{0.0f};
        Float _pad0{0.0f};
        UInt32 atlas_index{0};
        UInt32 _pad1{0};
        Float uv_min_x{0.0f};
        Float uv_min_y{0.0f};
        Float uv_max_x{1.0f};
        Float uv_max_y{1.0f};
        UInt32 color{0xFFFFFFFF};
        UInt32 sorting_key{0};
        UInt32 material_id{0};
        UInt32 flags{0};
    };

    static_assert(sizeof(SpriteInstance) == 64, "SpriteInstance must be 64 bytes");
    static_assert(alignof(SpriteInstance) == 16, "SpriteInstance must be 16-byte aligned");

    constexpr UInt32 kSpriteFlagFlipX        = 1 << 0;
    constexpr UInt32 kSpriteFlagFlipY        = 1 << 1;
    constexpr UInt32 kSpriteFlagHasNormalMap = 1 << 2;
    constexpr UInt32 kSpriteFlagCastShadow   = 1 << 3;
    constexpr UInt32 kSpriteFlagIsOpaque     = 1 << 4;

    inline constexpr Float kQuadVertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
    };

    inline constexpr UInt16 kQuadIndices[] = {0, 1, 2, 2, 3, 0};

    struct SpriteSceneInfo {
        DynamicArray<SpriteInstance> m_instances{};
        UInt32 m_instance_count{0};
        Bool m_dirty{true};

        void clear() {
            m_instances.clear();
            m_instance_count = 0;
            m_dirty = true;
        }
    };

} // dodoe
