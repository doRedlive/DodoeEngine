// do@Redlive

#pragma once

#include "dopch.h"

#include "cutie/cutie.h"
#include "cutie/vulkan.h"
#include "cutie/opengl.h"
#include "cutie/d3d12.h"
#include "cutie/validation.h"
#include "cutie/utils.h"

namespace dodoe {

    using GfxDevice = cutie::IDevice;
    using GfxCommandList = cutie::ICommandList;
    using GfxShader = cutie::IShader;
    using GfxSampler = cutie::ISampler;
    using GfxInputLayout = cutie::IInputLayout;
    using GfxBindingLayout = cutie::IBindingLayout;
    using GfxComputePipeline = cutie::IComputePipeline;
    using GfxDescriptorTable = cutie::IDescriptorTable;
    using GfxMessageCallback = cutie::IMessageCallback;

    using GfxDeviceHandle = cutie::DeviceHandle;
    using GfxCommandListHandle = cutie::CommandListHandle;
    using GfxCommandListLifetimeTracker = cutie::ICommandListLifetimeTracker;
    using GfxCommandListLifetimeTrackerHandle = cutie::CommandListLifetimeTrackerHandle;
    using GfxCommandListParameters = cutie::CommandListParameters;
    using GfxShaderHandle = cutie::ShaderHandle;
    using GfxSamplerHandle = cutie::SamplerHandle;
    using GfxInputLayoutHandle = cutie::InputLayoutHandle;
    using GfxBindingLayoutHandle = cutie::BindingLayoutHandle;
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
    using GfxBufferRange = cutie::BufferRange;
    using GfxShaderDesc = cutie::ShaderDesc;
    using GfxSamplerDesc = cutie::SamplerDesc;
    using GfxBindingLayoutDesc = cutie::BindingLayoutDesc;
    using GfxBindingSetDesc = cutie::BindingSetDesc;
    using GfxBindlessLayoutDesc = cutie::BindlessLayoutDesc;
    using GfxGraphicsPipelineDesc = cutie::GraphicsPipelineDesc;
    using GfxComputePipelineDesc = cutie::ComputePipelineDesc;
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
    using GfxBindingLayoutItem = cutie::BindingLayoutItem;
    using GfxBindingSetItem = cutie::BindingSetItem;
    using GfxVariableRateShadingState = cutie::VariableRateShadingState;
    using GfxEventQueryHandle = cutie::EventQueryHandle;
    using GfxCpuAccessMode = cutie::CpuAccessMode;

    namespace GfxObjectTypes = cutie::ObjectTypes;

    inline const auto& GfxAllSubresources = cutie::AllSubresources;

    using cutie::hash_combine;

    namespace validation = cutie::validation;
    namespace vulkan = cutie::vulkan;
    namespace opengl = cutie::opengl;
    namespace d3d12 = cutie::d3d12;

    class GfxTexture {
        cutie::TextureHandle m_rhi{};
        GfxTextureDesc m_desc{};
        String m_debug_name{};
        bool m_rhi_ready{false};
    public:
        GfxTexture() = default;
        explicit GfxTexture(const GfxTextureDesc& desc, const String& debug_name = "")
            : m_desc(desc), m_debug_name(debug_name) {}
        explicit GfxTexture(const cutie::TextureHandle& handle, const GfxTextureDesc& desc, const String& debug_name = "")
            : m_rhi(handle), m_desc(desc), m_debug_name(debug_name), m_rhi_ready(true) {}
        void initializeRHI(GfxDeviceHandle device) {
            if (!m_rhi_ready) { m_rhi = device->createTexture(m_desc); m_rhi_ready = true; }
        }
        [[nodiscard]] cutie::ITexture* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::TextureHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] GfxFormat getFormat() const { return m_desc.format; }
        [[nodiscard]] UInt32 getWidth() const { return m_desc.width; }
        [[nodiscard]] UInt32 getHeight() const { return m_desc.height; }
        [[nodiscard]] const GfxTextureDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const String& getDebugName() const { return m_debug_name; }
        [[nodiscard]] bool isRHIReady() const { return m_rhi_ready; }
    };
    using GfxTextureHandle = Ref<GfxTexture>;

    class GfxBuffer {
        cutie::BufferHandle m_rhi{};
        GfxBufferDesc m_desc{};
        String m_debug_name{};
        bool m_rhi_ready{false};
    public:
        GfxBuffer() = default;
        explicit GfxBuffer(const GfxBufferDesc& desc, const String& debug_name = "")
            : m_desc(desc), m_debug_name(debug_name) {}
        explicit GfxBuffer(const cutie::BufferHandle& handle, const GfxBufferDesc& desc, const String& debug_name = "")
            : m_rhi(handle), m_desc(desc), m_debug_name(debug_name), m_rhi_ready(true) {}
        void initializeRHI(GfxDeviceHandle device) {
            if (!m_rhi_ready) { m_rhi = device->createBuffer(m_desc); m_rhi_ready = true; }
        }
        [[nodiscard]] cutie::IBuffer* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::BufferHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] UInt32 getByteSize() const { return m_desc.byteSize; }
        [[nodiscard]] const GfxBufferDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const String& getDebugName() const { return m_debug_name; }
        [[nodiscard]] bool isRHIReady() const { return m_rhi_ready; }
    };
    using GfxBufferHandle = Ref<GfxBuffer>;

    class GfxFramebufferDesc {
        DynamicArray<GfxTextureHandle> m_colors{};
        GfxTextureHandle m_depth{};
    public:
        GfxFramebufferDesc& addColorAttachment(const GfxTextureHandle& tex) { m_colors.push_back(tex); return *this; }
        GfxFramebufferDesc& setDepthAttachment(const GfxTextureHandle& tex) { m_depth = tex; return *this; }
        [[nodiscard]] const DynamicArray<GfxTextureHandle>& colors() const { return m_colors; }
        [[nodiscard]] const GfxTextureHandle& depth() const { return m_depth; }
        [[nodiscard]] cutie::FramebufferDesc toRHI() const {
            cutie::FramebufferDesc desc;
            for (auto& c : m_colors) desc.addColorAttachment(c->getRHIHandle());
            if (m_depth) desc.setDepthAttachment(m_depth->getRHIHandle());
            return desc;
        }
    };

    class GfxFramebufferInfo {
        cutie::FramebufferInfo m_info{};
    public:
        GfxFramebufferInfo() = default;
        explicit GfxFramebufferInfo(const GfxFramebufferDesc& desc) {
            for (auto& c : desc.colors()) m_info.addColorFormat(c->getFormat());
            if (desc.depth()) m_info.setDepthFormat(desc.depth()->getFormat());
        }
        [[nodiscard]] const cutie::FramebufferInfo& getRHI() const { return m_info; }
        GfxFramebufferInfo& addColorFormat(GfxFormat f) { m_info.addColorFormat(f); return *this; }
        GfxFramebufferInfo& setDepthFormat(GfxFormat f) { m_info.setDepthFormat(f); return *this; }
    };

    class GfxFramebuffer {
        cutie::FramebufferHandle m_rhi{};
        GfxFramebufferDesc m_desc{};
        GfxFramebufferInfo m_info{};
        bool m_rhi_ready{false};
    public:
        GfxFramebuffer() = default;
        GfxFramebuffer(const cutie::FramebufferHandle& handle, const GfxFramebufferInfo& info)
            : m_rhi(handle), m_info(info), m_rhi_ready(true) {}
        explicit GfxFramebuffer(const GfxFramebufferDesc& desc, const GfxFramebufferInfo& info)
            : m_desc(desc), m_info(info) {}
        explicit GfxFramebuffer(const GfxFramebufferDesc& desc)
            : m_desc(desc), m_info(desc) {}
        void initializeRHI(GfxDeviceHandle device) {
            if (!m_rhi_ready) {
                m_rhi = device->createFramebuffer(m_desc.toRHI());
                m_rhi_ready = true;
            }
        }
        [[nodiscard]] cutie::IFramebuffer* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::FramebufferHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxFramebufferInfo& getInfo() const { return m_info; }
        [[nodiscard]] const GfxFramebufferInfo& getFramebufferInfo() const { return m_info; }
        [[nodiscard]] bool isRHIReady() const { return m_rhi_ready; }
    };
    using GfxFramebufferHandle = Ref<GfxFramebuffer>;

    class GfxGraphicsPipeline {
        cutie::GraphicsPipelineHandle m_rhi{};
        GfxGraphicsPipelineDesc m_desc{};
        GfxFramebufferInfo m_framebuffer_info{};
        bool m_rhi_ready{false};
    public:
        GfxGraphicsPipeline() = default;
        void initializeRHI(GfxDeviceHandle device, const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
            if (!m_rhi_ready) {
                m_desc = desc;
                m_framebuffer_info = info;
                m_rhi = device->createGraphicsPipeline(desc, info.getRHI());
                m_rhi_ready = true;
            }
        }
        [[nodiscard]] cutie::IGraphicsPipeline* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::GraphicsPipelineHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxGraphicsPipelineDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const GfxFramebufferInfo& getFramebufferInfo() const { return m_framebuffer_info; }
        [[nodiscard]] bool isRHIReady() const { return m_rhi_ready; }
    };
    using GfxGraphicsPipelineHandle = Ref<GfxGraphicsPipeline>;

    class GfxBindingSet {
        cutie::BindingSetHandle m_rhi{};
        GfxBindingSetDesc m_desc{};
        GfxBindingLayoutHandle m_layout{};
        bool m_rhi_ready{false};
    public:
        GfxBindingSet() = default;
        void initializeRHI(GfxDeviceHandle device, const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
            if (!m_rhi_ready) { m_desc = desc; m_layout = layout; m_rhi = device->createBindingSet(desc, layout); m_rhi_ready = true; }
        }
        explicit GfxBindingSet(const cutie::BindingSetHandle& handle, const GfxBindingSetDesc& desc = {}, const GfxBindingLayoutHandle& layout = {}) : m_rhi(handle), m_desc(desc), m_layout(layout), m_rhi_ready(true) {}
        [[nodiscard]] cutie::IBindingSet* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::BindingSetHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxBindingSetDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const GfxBindingLayoutHandle& getLayout() const { return m_layout; }
        [[nodiscard]] bool isRHIReady() const { return m_rhi_ready; }
    };
    using GfxBindingSetHandle = Ref<GfxBindingSet>;

} // dodoe
