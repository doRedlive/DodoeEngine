// do@Redlive

#pragma once

#include "dopch.h"

#include "nvrhi/nvrhi.h"
#include "nvrhi/vulkan.h"
#include "nvrhi/validation.h"
#include "nvrhi/utils.h"

#include "backend/vulkan_backend.h"

namespace dodoe {

    using GfxDevice = nvrhi::IDevice;
    using GfxCommandList = nvrhi::ICommandList;
    using GfxTexture = nvrhi::ITexture;
    using GfxBuffer = nvrhi::IBuffer;
    using GfxShader = nvrhi::IShader;
    using GfxSampler = nvrhi::ISampler;
    using GfxInputLayout = nvrhi::IInputLayout;
    using GfxBindingLayout = nvrhi::IBindingLayout;
    using GfxBindingSet = nvrhi::IBindingSet;
    using GfxFramebuffer = nvrhi::IFramebuffer;
    using GfxGraphicsPipeline = nvrhi::IGraphicsPipeline;
    using GfxComputePipeline = nvrhi::IComputePipeline;
    using GfxDescriptorTable = nvrhi::IDescriptorTable;
    using GfxMessageCallback = nvrhi::IMessageCallback;

    using GfxDeviceHandle = nvrhi::DeviceHandle;
    using GfxCommandListHandle = nvrhi::CommandListHandle;
    using GfxTextureHandle = nvrhi::TextureHandle;
    using GfxBufferHandle = nvrhi::BufferHandle;
    using GfxShaderHandle = nvrhi::ShaderHandle;
    using GfxSamplerHandle = nvrhi::SamplerHandle;
    using GfxInputLayoutHandle = nvrhi::InputLayoutHandle;
    using GfxBindingLayoutHandle = nvrhi::BindingLayoutHandle;
    using GfxBindingSetHandle = nvrhi::BindingSetHandle;
    using GfxFramebufferHandle = nvrhi::FramebufferHandle;
    using GfxGraphicsPipelineHandle = nvrhi::GraphicsPipelineHandle;
    using GfxComputePipelineHandle = nvrhi::ComputePipelineHandle;
    using GfxDescriptorTableHandle = nvrhi::DescriptorTableHandle;

    using GfxFormat = nvrhi::Format;
    using GfxColor = nvrhi::Color;
    using GfxRect = nvrhi::Rect;
    using GfxViewport = nvrhi::Viewport;
    using GfxViewportState = nvrhi::ViewportState;
    using GfxDrawArguments = nvrhi::DrawArguments;
    using GfxComputeState = nvrhi::ComputeState;
    using GfxGraphicsState = nvrhi::GraphicsState;
    using GfxTextureDesc = nvrhi::TextureDesc;
    using GfxBufferDesc = nvrhi::BufferDesc;
    using GfxShaderDesc = nvrhi::ShaderDesc;
    using GfxSamplerDesc = nvrhi::SamplerDesc;
    using GfxFramebufferDesc = nvrhi::FramebufferDesc;
    using GfxBindingLayoutDesc = nvrhi::BindingLayoutDesc;
    using GfxBindingSetDesc = nvrhi::BindingSetDesc;
    using GfxBindlessLayoutDesc = nvrhi::BindlessLayoutDesc;
    using GfxGraphicsPipelineDesc = nvrhi::GraphicsPipelineDesc;
    using GfxVertexAttributeDesc = nvrhi::VertexAttributeDesc;
    using GfxVertexBufferBinding = nvrhi::VertexBufferBinding;
    using GfxIndexBufferBinding = nvrhi::IndexBufferBinding;
    using GfxRenderState = nvrhi::RenderState;
    using GfxRasterState = nvrhi::RasterState;
    using GfxBlendState = nvrhi::BlendState;
    using GfxDepthStencilState = nvrhi::DepthStencilState;
    using GfxTextureSubresourceSet = nvrhi::TextureSubresourceSet;
    using GfxResourceStates = nvrhi::ResourceStates;
    using GfxPrimitiveType = nvrhi::PrimitiveType;
    using GfxTextureDimension = nvrhi::TextureDimension;
    using GfxShaderType = nvrhi::ShaderType;
    using GfxComparisonFunc = nvrhi::ComparisonFunc;
    using GfxBlendFactor = nvrhi::BlendFactor;
    using GfxBlendOp = nvrhi::BlendOp;
    using GfxCommandQueue = nvrhi::CommandQueue;
    using GfxMessageSeverity = nvrhi::MessageSeverity;
    namespace GfxObjectTypes = nvrhi::ObjectTypes;
    using GfxBindingLayoutItem = nvrhi::BindingLayoutItem;
    using GfxBindingSetItem = nvrhi::BindingSetItem;
    using GfxFramebufferInfo = nvrhi::FramebufferInfo;
    using GfxVariableRateShadingState = nvrhi::VariableRateShadingState;

    inline const auto& GfxAllSubresources = nvrhi::AllSubresources;

    using nvrhi::hash_combine;

    namespace validation = nvrhi::validation;
    namespace vulkan = nvrhi::vulkan;

} // dodoe
