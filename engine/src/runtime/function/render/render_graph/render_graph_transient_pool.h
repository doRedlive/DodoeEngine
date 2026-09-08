// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class DrawCommandList;

    class RenderGraphTransientPool {
    public:
        RenderGraphTransientPool() = default;

        GfxTextureHandle acquireTexture(const GfxTextureDesc& desc,
                                         DrawCommandList& command_list);
        GfxBufferHandle acquireBuffer(const GfxBufferDesc& desc,
                                       DrawCommandList& command_list);
        void releaseAll();
        void reset();

#ifdef DODOE_PERF_ENABLED
        struct GlobalStats {
            Size_t texture_count{0};
            UInt64 texture_bytes{0};
            Size_t buffer_count{0};
            UInt64 buffer_bytes{0};
        };
        static GlobalStats QueryGlobalStats();
#endif

    private:
        struct PooledTexture {
            GfxTextureHandle texture{};
            GfxTextureDesc desc{};
        };
        struct PooledBuffer {
            GfxBufferHandle buffer{};
            GfxBufferDesc desc{};
        };
        DynamicArray<PooledTexture> m_textures{};
        DynamicArray<PooledBuffer> m_buffers{};
        DynamicArray<UInt8> m_texture_in_use{};
        DynamicArray<UInt8> m_buffer_in_use{};
    };

} // namespace dodoe
