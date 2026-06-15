// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_resource.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderGraphResourceRegistry {
        DynamicArray<GfxTextureHandle> m_texture_handles{};
        DynamicArray<GfxBufferHandle> m_buffer_handles{};

    public:
        RenderGraphResourceRegistry() = default;

        void initialize(const DynamicArray<RenderGraphResourceRecord>& resources, GfxContext& gfx_context, const UInt32 swapchain_image_index);
        void reset();

        [[nodiscard]] GfxTextureHandle getTexture(const RenderGraphTextureHandle handle) const;
        [[nodiscard]] GfxBufferHandle getBuffer(const RenderGraphBufferHandle handle) const;
    };

} // dodoe
