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

    using Format = nvrhi::Format;
    using Color = nvrhi::Color;
    using Rect = nvrhi::Rect;
    using Viewport = nvrhi::Viewport;
    using ViewportState = nvrhi::ViewportState;
    using DrawArguments = nvrhi::DrawArguments;
    using ComputeState = nvrhi::ComputeState;
    using GraphicsState = nvrhi::GraphicsState;
    using TextureDesc = nvrhi::TextureDesc;
    using BufferDesc = nvrhi::BufferDesc;
    using ShaderDesc = nvrhi::ShaderDesc;
    using SamplerDesc = nvrhi::SamplerDesc;
    using FramebufferDesc = nvrhi::FramebufferDesc;
    using BindingLayoutDesc = nvrhi::BindingLayoutDesc;
    using BindingSetDesc = nvrhi::BindingSetDesc;
    using BindlessLayoutDesc = nvrhi::BindlessLayoutDesc;
    using GraphicsPipelineDesc = nvrhi::GraphicsPipelineDesc;
    using VertexAttributeDesc = nvrhi::VertexAttributeDesc;
    using VertexBufferBinding = nvrhi::VertexBufferBinding;
    using IndexBufferBinding = nvrhi::IndexBufferBinding;
    using RenderState = nvrhi::RenderState;
    using RasterState = nvrhi::RasterState;
    using BlendState = nvrhi::BlendState;
    using DepthStencilState = nvrhi::DepthStencilState;
    using TextureSubresourceSet = nvrhi::TextureSubresourceSet;
    using ResourceStates = nvrhi::ResourceStates;
    using PrimitiveType = nvrhi::PrimitiveType;
    using TextureDimension = nvrhi::TextureDimension;
    using ShaderType = nvrhi::ShaderType;
    using ComparisonFunc = nvrhi::ComparisonFunc;
    using BlendFactor = nvrhi::BlendFactor;
    using BlendOp = nvrhi::BlendOp;
    using CommandQueue = nvrhi::CommandQueue;
    using MessageSeverity = nvrhi::MessageSeverity;
    using ObjectTypes = nvrhi::ObjectTypes;
    using BindingLayoutItem = nvrhi::BindingLayoutItem;
    using BindingSetItem = nvrhi::BindingSetItem;

    inline constexpr auto AllSubresources = nvrhi::AllSubresources;

    using nvrhi::hash_combine;

    namespace validation = nvrhi::validation;
    namespace vulkan = nvrhi::vulkan;

} // dodoe
