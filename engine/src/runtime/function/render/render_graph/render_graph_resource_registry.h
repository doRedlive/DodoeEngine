// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_resource.h"
#include "runtime/function/render/render_service/render_graph_transient_pool.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class DrawCommandList;

    using TransientResourcePool = RenderGraphTransientPool;

    class RenderGraphResourceRegistry {
        DynamicArray<GfxTextureHandle> m_texture_handles{};
        DynamicArray<GfxBufferHandle> m_buffer_handles{};
        RenderGraphTransientPool* m_transient_pool{nullptr};

    public:
        RenderGraphResourceRegistry(const DynamicArray<RenderGraphResourceRecord>& resources, GfxContext& gfx_context,
            const UInt32 swapchain_image_index, DrawCommandList& command_list,
            RenderGraphTransientPool* transient_pool = nullptr);

        void reset();

        [[nodiscard]] GfxTextureHandle getTexture(const RenderGraphTextureHandle handle) const;
        [[nodiscard]] GfxBufferHandle getBuffer(const RenderGraphBufferHandle handle) const;
    };

} // dodoe
