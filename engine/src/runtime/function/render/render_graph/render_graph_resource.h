// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    inline constexpr UInt32 kInvalidRenderGraphHandle = static_cast<UInt32>(-1);

    enum class RenderGraphResourceType {
        Texture = 0,
        Buffer,
    };

    enum class RenderGraphResourceSource {
        Transient = 0,
        ImportedTexture,
        ImportedBuffer,
        ImportedBackBuffer,
    };

    enum class RenderGraphAccessType {
        Read = 0,
        Write,
    };

    struct RenderGraphTextureDesc {
        GfxTextureDesc desc{};
    };

    struct RenderGraphBufferDesc {
        GfxBufferDesc desc{};
    };

    struct RenderGraphTextureHandle {
        UInt32 index{kInvalidRenderGraphHandle};

        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphBufferHandle {
        UInt32 index{kInvalidRenderGraphHandle};

        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphPassResourceAccess {
        UInt32 resource_index{kInvalidRenderGraphHandle};
        RenderGraphAccessType access_type{RenderGraphAccessType::Read};
    };

    struct RenderGraphResourceRecord {
        RenderGraphResourceType type{RenderGraphResourceType::Texture};
        RenderGraphResourceSource source{RenderGraphResourceSource::Transient};
        String name{};
        RenderGraphTextureDesc texture_desc{};
        RenderGraphBufferDesc buffer_desc{};
        GfxTextureHandle imported_texture{};
        GfxBufferHandle imported_buffer{};
        Int32 producer_pass_index{-1};
        Int32 first_pass_index{-1};
        Int32 last_pass_index{-1};
        DynamicArray<UInt32> reader_passes{};

        [[nodiscard]] Bool isImported() const {
            return source != RenderGraphResourceSource::Transient;
        }
    };

} // dodoe