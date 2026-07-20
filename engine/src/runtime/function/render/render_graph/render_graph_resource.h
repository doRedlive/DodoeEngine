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

    enum class RenderGraphAccessType : UInt8 {
        Read = 0,
        Write,
        ReadWrite,
    };

    enum class RenderGraphPipelineStage : UInt8 {
        VertexShader = 0,
        PixelShader,
        ComputeShader,
        Copy,
        RenderTarget,
        DepthStencil,
    };

    struct RenderGraphSubresourceRange {
        UInt32 base_mip{0};
        UInt32 mip_count{1};
        UInt32 base_array_layer{0};
        UInt32 array_layer_count{1};
    };

    enum class LoadOp : UInt8 {
        Load,
        Clear,
        DontCare,
    };

    enum class StoreOp : UInt8 {
        Store,
        DontCare,
    };

    struct RenderGraphAttachmentInfo {
        LoadOp load_op{LoadOp::DontCare};
        StoreOp store_op{StoreOp::Store};
        GfxColor clear_color{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct RenderGraphAccessInfo {
        RenderGraphAccessType access_type{RenderGraphAccessType::Read};
        RenderGraphPipelineStage stage{RenderGraphPipelineStage::PixelShader};
        GfxResourceStates required_state{GfxResourceStates::Unknown};
        RenderGraphSubresourceRange subresource{};
    };

    struct RenderGraphTextureDesc {
        GfxTextureDesc desc{};
    };

    struct RenderGraphBufferDesc {
        GfxBufferDesc desc{};
    };

    inline RenderGraphTextureDesc MakeRenderTarget2D(
        const UInt32 width, const UInt32 height,
        const GfxFormat format,
        const String& debug_name)
    {
        RenderGraphTextureDesc desc{};
        desc.desc.setWidth(width)
            .setHeight(height)
            .setFormat(format)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .setDebugName(debug_name);
        return desc;
    }

    inline RenderGraphTextureDesc MakeDepthTarget2D(
        const UInt32 width, const UInt32 height,
        const GfxFormat format,
        const String& debug_name)
    {
        RenderGraphTextureDesc desc{};
        desc.desc.setWidth(width)
            .setHeight(height)
            .setFormat(format)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
            .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
            .setDebugName(debug_name);
        return desc;
    }

    struct RenderGraphTextureHandle {
        UInt32 index{kInvalidRenderGraphHandle};

        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphBufferHandle {
        UInt32 index{kInvalidRenderGraphHandle};

        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphBarrier {
        UInt32 resource_index{kInvalidRenderGraphHandle};
        RenderGraphResourceType resource_type{RenderGraphResourceType::Texture};
        GfxResourceStates from_state{GfxResourceStates::Unknown};
        GfxResourceStates to_state{GfxResourceStates::Unknown};
        RenderGraphSubresourceRange subresource{};
    };

    struct RenderGraphPassResourceAccess {
        UInt32 resource_index{kInvalidRenderGraphHandle};
        RenderGraphAccessType access_type{RenderGraphAccessType::Read};
        RenderGraphPipelineStage stage{RenderGraphPipelineStage::PixelShader};
        RenderGraphSubresourceRange subresource{};
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
        Bool is_exported{false};
        GfxResourceStates export_final_state{GfxResourceStates::Unknown};

        [[nodiscard]] Bool isImported() const {
            return source != RenderGraphResourceSource::Transient;
        }
    };

} // dodoe