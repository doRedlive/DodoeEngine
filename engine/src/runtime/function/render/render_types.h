#pragma once

#include "dopch.h"

namespace dodoe {

struct alignas(16) SpriteInstance {
    float  position_x{0.0f};
    float  position_y{0.0f};
    float  scale_x{1.0f};
    float  scale_y{1.0f};
    float  rotation{0.0f};
    float  _pad0{0.0f};
    uint32 atlas_index{0};
    uint32 _pad1{0};
    float  uv_min_x{0.0f};
    float  uv_min_y{0.0f};
    float  uv_max_x{1.0f};
    float  uv_max_y{1.0f};
    uint32 color{0xFFFFFFFF};
    uint32 sorting_key{0};
    uint32 material_id{0};
    uint32 flags{0};
};

static_assert(sizeof(SpriteInstance) == 64, "SpriteInstance must be 64 bytes");
static_assert(alignof(SpriteInstance) == 16, "SpriteInstance must be 16-byte aligned");

namespace SpriteFlags {
    constexpr uint32 None           = 0;
    constexpr uint32 FlipX          = 1 << 0;
    constexpr uint32 FlipY          = 1 << 1;
    constexpr uint32 HasNormalMap   = 1 << 2;
    constexpr uint32 CastShadow     = 1 << 3;
    constexpr uint32 IsOpaque       = 1 << 4;
}

struct QuadVertex {
    float position[3]{};
    float uv[2]{};
};

inline constexpr QuadVertex kQuadVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}},
};

inline constexpr uint16_t kQuadIndices[] = {0, 1, 2, 2, 3, 0};

struct SpriteSceneProxy {
    DynamicArray<SpriteInstance> instances{};
    uint32_t instance_count{0};
    bool dirty{true};

    void clear() {
        instances.clear();
        instance_count = 0;
        dirty = true;
    }
};

} // namespace dodoe
