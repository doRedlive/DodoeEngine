// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_resource.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class DrawCommandList;

    class TransientResourcePool {
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

    public:
        GfxTextureHandle acquireTexture(const GfxTextureDesc& desc, DrawCommandList& command_list);
        GfxBufferHandle acquireBuffer(const GfxBufferDesc& desc, DrawCommandList& command_list);
        void releaseAll();
        void reset();
    };

    class RenderGraphResourceRegistry {
        DynamicArray<GfxTextureHandle> m_texture_handles{};
        DynamicArray<GfxBufferHandle> m_buffer_handles{};
        TransientResourcePool* m_transient_pool{nullptr};

    public:
        RenderGraphResourceRegistry(const DynamicArray<RenderGraphResourceRecord>& resources, GfxContext& gfx_context,
            const UInt32 swapchain_image_index, DrawCommandList& command_list,
            TransientResourcePool* transient_pool = nullptr);

        void reset();

        [[nodiscard]] GfxTextureHandle getTexture(const RenderGraphTextureHandle handle) const;
        [[nodiscard]] GfxBufferHandle getBuffer(const RenderGraphBufferHandle handle) const;
    };

} // dodoe
