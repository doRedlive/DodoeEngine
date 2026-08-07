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

    struct RenderGraphTextureHandle {
        UInt32 index{kInvalidRenderGraphHandle};
        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphBufferHandle {
        UInt32 index{kInvalidRenderGraphHandle};
        [[nodiscard]] Bool isValid() const { return index != kInvalidRenderGraphHandle; }
    };

    struct RenderGraphColorAttachment {
        RenderGraphTextureHandle texture{};
        LoadOp load_op{LoadOp::DontCare};
        StoreOp store_op{StoreOp::Store};
        GfxColor clear_color{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct RenderGraphDepthStencilAttachment {
        RenderGraphTextureHandle texture{};
        LoadOp depth_load_op{LoadOp::DontCare};
        StoreOp depth_store_op{StoreOp::Store};
        LoadOp stencil_load_op{LoadOp::DontCare};
        StoreOp stencil_store_op{StoreOp::DontCare};
        Float clear_depth{1.0f};
        UInt8 clear_stencil{0};
    };

    static constexpr UInt32 kRenderGraphMaxColorAttachments = 8;

    struct RenderGraphRenderTargetBindingSlots {
        RenderGraphColorAttachment color[kRenderGraphMaxColorAttachments]{};
        RenderGraphDepthStencilAttachment depth{};
        UInt32 color_count{0};
        Bool has_depth{false};

        [[nodiscard]] RenderGraphColorAttachment& operator[](const UInt32 index) {
            DO_ASSERT(index < kRenderGraphMaxColorAttachments, "RenderGraphRenderTargetBindingSlots index out of range");
            return color[index];
        }

        [[nodiscard]] const RenderGraphColorAttachment& operator[](const UInt32 index) const {
            DO_ASSERT(index < kRenderGraphMaxColorAttachments, "RenderGraphRenderTargetBindingSlots index out of range");
            return color[index];
        }
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
            .setDebugName(string_to_std(debug_name));
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
            .setDebugName(string_to_std(debug_name));
        return desc;
    }

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
        DynamicArray<UInt32> writer_passes{};
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