// do@Redlive

#pragma once

#include "dopch.h"

#include "cutie/cutie.h"
#include "cutie/vulkan.h"
#include "cutie/opengl.h"
#include "cutie/validation.h"
#include "cutie/utils.h"

namespace dodoe {

    using GfxDevice = cutie::IDevice;
    using GfxCommandList = cutie::ICommandList;
    using GfxTexture = cutie::ITexture;
    using GfxBuffer = cutie::IBuffer;
    using GfxShader = cutie::IShader;
    using GfxSampler = cutie::ISampler;
    using GfxInputLayout = cutie::IInputLayout;
    using GfxBindingLayout = cutie::IBindingLayout;
    using GfxBindingSet = cutie::IBindingSet;
    using GfxFramebuffer = cutie::IFramebuffer;
    using GfxGraphicsPipeline = cutie::IGraphicsPipeline;
    using GfxComputePipeline = cutie::IComputePipeline;
    using GfxDescriptorTable = cutie::IDescriptorTable;
    using GfxMessageCallback = cutie::IMessageCallback;

    using GfxDeviceHandle = cutie::DeviceHandle;
    using GfxCommandListHandle = cutie::CommandListHandle;
    using GfxTextureHandle = cutie::TextureHandle;
    using GfxBufferHandle = cutie::BufferHandle;
    using GfxShaderHandle = cutie::ShaderHandle;
    using GfxSamplerHandle = cutie::SamplerHandle;
    using GfxInputLayoutHandle = cutie::InputLayoutHandle;
    using GfxBindingLayoutHandle = cutie::BindingLayoutHandle;
    using GfxBindingSetHandle = cutie::BindingSetHandle;
    using GfxFramebufferHandle = cutie::FramebufferHandle;
    using GfxGraphicsPipelineHandle = cutie::GraphicsPipelineHandle;
    using GfxComputePipelineHandle = cutie::ComputePipelineHandle;
    using GfxDescriptorTableHandle = cutie::DescriptorTableHandle;

    using GfxFormat = cutie::Format;
    using GfxColor = cutie::Color;
    using GfxRect = cutie::Rect;
    using GfxViewport = cutie::Viewport;
    using GfxViewportState = cutie::ViewportState;
    using GfxDrawArguments = cutie::DrawArguments;
    using GfxComputeState = cutie::ComputeState;
    using GfxGraphicsState = cutie::GraphicsState;
    using GfxTextureDesc = cutie::TextureDesc;
    using GfxBufferDesc = cutie::BufferDesc;
    using GfxShaderDesc = cutie::ShaderDesc;
    using GfxSamplerDesc = cutie::SamplerDesc;
    using GfxFramebufferDesc = cutie::FramebufferDesc;
    using GfxBindingLayoutDesc = cutie::BindingLayoutDesc;
    using GfxBindingSetDesc = cutie::BindingSetDesc;
    using GfxBindlessLayoutDesc = cutie::BindlessLayoutDesc;
    using GfxGraphicsPipelineDesc = cutie::GraphicsPipelineDesc;
    using GfxVertexAttributeDesc = cutie::VertexAttributeDesc;
    using GfxVertexBufferBinding = cutie::VertexBufferBinding;
    using GfxIndexBufferBinding = cutie::IndexBufferBinding;
    using GfxRenderState = cutie::RenderState;
    using GfxRasterState = cutie::RasterState;
    using GfxBlendState = cutie::BlendState;
    using GfxDepthStencilState = cutie::DepthStencilState;
    using GfxTextureSubresourceSet = cutie::TextureSubresourceSet;
    using GfxResourceStates = cutie::ResourceStates;
    using GfxPrimitiveType = cutie::PrimitiveType;
    using GfxTextureDimension = cutie::TextureDimension;
    using GfxShaderType = cutie::ShaderType;
    using GfxComparisonFunc = cutie::ComparisonFunc;
    using GfxBlendFactor = cutie::BlendFactor;
    using GfxBlendOp = cutie::BlendOp;
    using GfxCommandQueue = cutie::CommandQueue;
    using GfxMessageSeverity = cutie::MessageSeverity;
    using GfxSamplerAddressMode = cutie::SamplerAddressMode;
    using GfxSamplerReductionType = cutie::SamplerReductionType;
    namespace GfxObjectTypes = cutie::ObjectTypes;
    using GfxBindingLayoutItem = cutie::BindingLayoutItem;
    using GfxBindingSetItem = cutie::BindingSetItem;
    using GfxFramebufferInfo = cutie::FramebufferInfo;
    using GfxVariableRateShadingState = cutie::VariableRateShadingState;

    inline const auto& GfxAllSubresources = cutie::AllSubresources;

    using cutie::hash_combine;

    namespace validation = cutie::validation;
    namespace vulkan = cutie::vulkan;
    namespace opengl = cutie::opengl;

} // dodoe
