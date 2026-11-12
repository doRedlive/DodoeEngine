// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class DrawCommandList;
    class FramebufferCache;
    class BindingSetCache;

    class RenderGraphTransientPool {
    public:
        static constexpr UInt64 kDefaultTextureBudgetBytes = 512ull * 1024 * 1024;
        static constexpr UInt64 kDefaultBufferBudgetBytes = 256ull * 1024 * 1024;

        RenderGraphTransientPool() = default;

        GfxTextureHandle acquireTexture(const GfxTextureDesc& desc,
                                         DrawCommandList& command_list);
        GfxBufferHandle acquireBuffer(const GfxBufferDesc& desc,
                                       DrawCommandList& command_list);
        void releaseAll();
        void reset();

        void setTextureBudgetBytes(const UInt64 bytes) { m_texture_budget_bytes = bytes; }
        void setBufferBudgetBytes(const UInt64 bytes) { m_buffer_budget_bytes = bytes; }
        [[nodiscard]] UInt64 getTextureBudgetBytes() const { return m_texture_budget_bytes; }
        [[nodiscard]] UInt64 getBufferBudgetBytes() const { return m_buffer_budget_bytes; }
        [[nodiscard]] UInt64 getTextureBytes() const { return m_texture_bytes; }
        [[nodiscard]] UInt64 getBufferBytes() const { return m_buffer_bytes; }

        void flushEvictions(FramebufferCache* framebuffer_cache, BindingSetCache* binding_set_cache);

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
            UInt64 bytes{0};
            UInt64 stamp{0};
        };
        struct PooledBuffer {
            GfxBufferHandle buffer{};
            GfxBufferDesc desc{};
            UInt64 bytes{0};
            UInt64 stamp{0};
        };

        void enforceTextureBudget();
        void enforceBufferBudget();

        DynamicArray<PooledTexture> m_textures{};
        DynamicArray<PooledBuffer> m_buffers{};
        DynamicArray<UInt8> m_texture_in_use{};
        DynamicArray<UInt8> m_buffer_in_use{};
        DynamicArray<GfxTextureHandle> m_pending_eviction_textures{};
        DynamicArray<GfxBufferHandle> m_pending_eviction_buffers{};
        UInt64 m_texture_budget_bytes{kDefaultTextureBudgetBytes};
        UInt64 m_buffer_budget_bytes{kDefaultBufferBudgetBytes};
        UInt64 m_texture_bytes{0};
        UInt64 m_buffer_bytes{0};
        UInt64 m_stamp{0};
    };

} // namespace dodoe
