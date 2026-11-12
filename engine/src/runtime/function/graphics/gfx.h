// do@Redlive

#pragma once

#include "dopch.h"

#include "cutie/cutie.h"
#include "cutie/vulkan.h"
#include "cutie/opengl.h"
#include "cutie/d3d12.h"
#include "cutie/validation.h"
#include "cutie/utils.h"

#include <atomic>

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
    using GfxStagingTextureHandle = cutie::StagingTextureHandle;

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
    using GfxTextureSlice = cutie::TextureSlice;
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
    using GfxTimerQuery = cutie::TimerQueryHandle;
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
        std::atomic<bool> m_gpu_ready{false};
    public:
        GfxTexture() = default;
        explicit GfxTexture(const GfxTextureDesc& desc, const String& debug_name = "")
            : m_desc(desc), m_debug_name(debug_name) {}
        explicit GfxTexture(const cutie::TextureHandle& handle, const GfxTextureDesc& desc, const String& debug_name = "")
            : m_rhi(handle), m_desc(desc), m_debug_name(debug_name), m_gpu_ready(true) {}
        void initializeGpu(GfxDeviceHandle device) {
            if (!m_gpu_ready) {
                m_rhi = device->createTexture(m_desc);
                m_gpu_ready = m_rhi != nullptr;
            }
        }
        [[nodiscard]] cutie::ITexture* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::TextureHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] GfxFormat getFormat() const { return m_desc.format; }
        [[nodiscard]] UInt32 getWidth() const { return m_desc.width; }
        [[nodiscard]] UInt32 getHeight() const { return m_desc.height; }
        [[nodiscard]] const GfxTextureDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const String& getDebugName() const { return m_debug_name; }
        [[nodiscard]] bool isGpuReady() const { return m_gpu_ready.load(std::memory_order_acquire); }
    };
    using GfxTextureHandle = Ref<GfxTexture>;

    class GfxBuffer {
        cutie::BufferHandle m_rhi{};
        GfxBufferDesc m_desc{};
        String m_debug_name{};
        std::atomic<bool> m_gpu_ready{false};
    public:
        GfxBuffer() = default;
        explicit GfxBuffer(const GfxBufferDesc& desc, const String& debug_name = "")
            : m_desc(desc), m_debug_name(debug_name) {}
        explicit GfxBuffer(const cutie::BufferHandle& handle, const GfxBufferDesc& desc, const String& debug_name = "")
            : m_rhi(handle), m_desc(desc), m_debug_name(debug_name), m_gpu_ready(true) {}
        void initializeGpu(GfxDeviceHandle device) {
            if (!m_gpu_ready) {
                m_rhi = device->createBuffer(m_desc);
                m_gpu_ready = m_rhi != nullptr;
            }
        }
        [[nodiscard]] cutie::IBuffer* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::BufferHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] UInt64 getByteSize() const { return m_desc.byteSize; }
        [[nodiscard]] const GfxBufferDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const String& getDebugName() const { return m_debug_name; }
        [[nodiscard]] bool isGpuReady() const { return m_gpu_ready.load(std::memory_order_acquire); }
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
            const auto attachment = !desc.colors().empty() ? desc.colors().front() : desc.depth();
            if (attachment) m_info.setSampleCount(attachment->getDesc().sampleCount);
        }
        [[nodiscard]] const cutie::FramebufferInfo& getRHI() const { return m_info; }
        GfxFramebufferInfo& addColorFormat(GfxFormat f) { m_info.addColorFormat(f); return *this; }
        GfxFramebufferInfo& setDepthFormat(GfxFormat f) { m_info.setDepthFormat(f); return *this; }
        GfxFramebufferInfo& setSampleCount(UInt32 count) { m_info.setSampleCount(count); return *this; }
        [[nodiscard]] UInt32 getSampleCount() const { return m_info.sampleCount; }
    };

    class GfxFramebuffer {
        cutie::FramebufferHandle m_rhi{};
        GfxFramebufferDesc m_desc{};
        GfxFramebufferInfo m_info{};
        std::atomic<bool> m_gpu_ready{false};
    public:
        GfxFramebuffer() = default;
        GfxFramebuffer(const cutie::FramebufferHandle& handle, const GfxFramebufferInfo& info)
            : m_rhi(handle), m_info(info), m_gpu_ready(true) {}
        explicit GfxFramebuffer(const GfxFramebufferDesc& desc, const GfxFramebufferInfo& info)
            : m_desc(desc), m_info(info) {}
        explicit GfxFramebuffer(const GfxFramebufferDesc& desc)
            : m_desc(desc), m_info(desc) {}
        void initializeGpu(GfxDeviceHandle device) {
            if (!m_gpu_ready) {
                m_rhi = device->createFramebuffer(m_desc.toRHI());
                m_gpu_ready = m_rhi != nullptr;
            }
        }
        [[nodiscard]] cutie::IFramebuffer* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::FramebufferHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxFramebufferInfo& getInfo() const { return m_info; }
        [[nodiscard]] const GfxFramebufferDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const GfxFramebufferInfo& getFramebufferInfo() const { return m_info; }
        [[nodiscard]] bool isGpuReady() const { return m_gpu_ready.load(std::memory_order_acquire); }
    };
    using GfxFramebufferHandle = Ref<GfxFramebuffer>;

    class GfxGraphicsPipeline {
        cutie::GraphicsPipelineHandle m_rhi{};
        GfxGraphicsPipelineDesc m_desc{};
        GfxFramebufferInfo m_framebuffer_info{};
        std::atomic<bool> m_gpu_ready{false};
    public:
        GfxGraphicsPipeline() = default;
        void setPendingDesc(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
            if (!m_gpu_ready) {
                m_desc = desc;
                m_framebuffer_info = info;
            }
        }
        void initializeGpu(GfxDeviceHandle device, const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
            setPendingDesc(desc, info);
            initializeGpu(device);
        }
        void initializeGpu(GfxDeviceHandle device) {
            if (!m_gpu_ready) {
                m_rhi = device->createGraphicsPipeline(m_desc, m_framebuffer_info.getRHI());
                m_gpu_ready = m_rhi != nullptr;
            }
        }
        [[nodiscard]] cutie::IGraphicsPipeline* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::GraphicsPipelineHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxGraphicsPipelineDesc& getDesc() const { return m_desc; }
        [[nodiscard]] const GfxFramebufferInfo& getFramebufferInfo() const { return m_framebuffer_info; }
        [[nodiscard]] bool isGpuReady() const { return m_gpu_ready.load(std::memory_order_acquire); }
    };
    using GfxGraphicsPipelineHandle = Ref<GfxGraphicsPipeline>;

    class GfxBindingSet;
    using GfxBindingSetHandle = Ref<GfxBindingSet>;

    struct GfxLogicalBindingItem {
        cutie::ResourceType type{cutie::ResourceType::None};
        UInt32 slot{0};
        UInt32 array_element{0};
        GfxTextureHandle texture{};
        GfxBufferHandle buffer{};
        GfxSamplerHandle sampler{};
        GfxFormat format{GfxFormat::UNKNOWN};
        GfxTextureDimension dimension{GfxTextureDimension::Unknown};
        UInt64 raw_data[2]{0, 0};

        [[nodiscard]] cutie::BindingSetItem toRHI() const {
            cutie::BindingSetItem r;
            r.slot = slot;
            r.arrayElement = array_element;
            r.type = type;
            r.resourceHandle = nullptr;
            r.format = format;
            r.dimension = dimension;
            r.rawData[0] = raw_data[0];
            r.rawData[1] = raw_data[1];
            r.unused = 0;
            r.unused2 = 0;
            switch (type) {
                case cutie::ResourceType::Texture_SRV:
                case cutie::ResourceType::Texture_UAV:
                    if (texture) r.resourceHandle = texture->getRHIHandle().Get();
                    break;
                case cutie::ResourceType::TypedBuffer_SRV:
                case cutie::ResourceType::TypedBuffer_UAV:
                case cutie::ResourceType::StructuredBuffer_SRV:
                case cutie::ResourceType::StructuredBuffer_UAV:
                case cutie::ResourceType::RawBuffer_SRV:
                case cutie::ResourceType::RawBuffer_UAV:
                case cutie::ResourceType::ConstantBuffer:
                case cutie::ResourceType::VolatileConstantBuffer:
                    if (buffer) r.resourceHandle = buffer->getRHIHandle().Get();
                    break;
                case cutie::ResourceType::Sampler:
                    r.resourceHandle = sampler.Get();
                    break;
                default:
                    break;
            }
            return r;
        }

        [[nodiscard]] Bool isResolved(GfxDeviceHandle device) const {
            (void)device;
            switch (type) {
                case cutie::ResourceType::Texture_SRV:
                case cutie::ResourceType::Texture_UAV:
                    return texture && texture->isGpuReady();
                case cutie::ResourceType::TypedBuffer_SRV:
                case cutie::ResourceType::TypedBuffer_UAV:
                case cutie::ResourceType::StructuredBuffer_SRV:
                case cutie::ResourceType::StructuredBuffer_UAV:
                case cutie::ResourceType::RawBuffer_SRV:
                case cutie::ResourceType::RawBuffer_UAV:
                case cutie::ResourceType::ConstantBuffer:
                case cutie::ResourceType::VolatileConstantBuffer:
                    return buffer && buffer->isGpuReady();
                case cutie::ResourceType::Sampler:
                    return sampler != nullptr;
                case cutie::ResourceType::PushConstants:
                case cutie::ResourceType::None:
                    return true;
                default:
                    return true;
            }
        }

        static GfxLogicalBindingItem MakeBase(cutie::BindingSetItem prototype) {
            GfxLogicalBindingItem item;
            item.slot = prototype.slot;
            item.array_element = prototype.arrayElement;
            item.type = prototype.type;
            item.format = prototype.format;
            item.dimension = prototype.dimension;
            item.raw_data[0] = prototype.rawData[0];
            item.raw_data[1] = prototype.rawData[1];
            return item;
        }

        static GfxLogicalBindingItem None(UInt32 slot = 0) {
            GfxLogicalBindingItem item = MakeBase(cutie::BindingSetItem::None(slot));
            return item;
        }

        static GfxLogicalBindingItem Texture_SRV(UInt32 slot, const GfxTextureHandle& texture,
            GfxFormat format = GfxFormat::UNKNOWN,
            GfxTextureSubresourceSet subresources = GfxAllSubresources,
            GfxTextureDimension dimension = GfxTextureDimension::Unknown) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::Texture_SRV(slot, nullptr, format, subresources, dimension));
            item.texture = texture;
            return item;
        }

        static GfxLogicalBindingItem Texture_UAV(UInt32 slot, const GfxTextureHandle& texture,
            GfxFormat format = GfxFormat::UNKNOWN,
            GfxTextureSubresourceSet subresources = GfxTextureSubresourceSet(0, 1, 0, GfxTextureSubresourceSet::AllArraySlices),
            GfxTextureDimension dimension = GfxTextureDimension::Unknown) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::Texture_UAV(slot, nullptr, format, subresources, dimension));
            item.texture = texture;
            return item;
        }

        static GfxLogicalBindingItem TypedBuffer_SRV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxFormat format = GfxFormat::UNKNOWN, GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::TypedBuffer_SRV(slot, nullptr, format, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem TypedBuffer_UAV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxFormat format = GfxFormat::UNKNOWN, GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::TypedBuffer_UAV(slot, nullptr, format, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem ConstantBuffer(UInt32 slot, const GfxBufferHandle& buffer,
            GfxBufferRange range = cutie::EntireBuffer) {
            const Bool is_volatile = buffer && buffer->getDesc().isVolatile;
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::ConstantBuffer(slot, nullptr, range));
            if (is_volatile) item.type = cutie::ResourceType::VolatileConstantBuffer;
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem Sampler(UInt32 slot, const GfxSamplerHandle& sampler) {
            GfxLogicalBindingItem item = MakeBase(cutie::BindingSetItem::Sampler(slot, nullptr));
            item.sampler = sampler;
            return item;
        }

        static GfxLogicalBindingItem StructuredBuffer_SRV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxFormat format = GfxFormat::UNKNOWN, GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::StructuredBuffer_SRV(slot, nullptr, format, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem StructuredBuffer_UAV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxFormat format = GfxFormat::UNKNOWN, GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::StructuredBuffer_UAV(slot, nullptr, format, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem RawBuffer_SRV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::RawBuffer_SRV(slot, nullptr, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem RawBuffer_UAV(UInt32 slot, const GfxBufferHandle& buffer,
            GfxBufferRange range = cutie::EntireBuffer) {
            GfxLogicalBindingItem item = MakeBase(
                cutie::BindingSetItem::RawBuffer_UAV(slot, nullptr, range));
            item.buffer = buffer;
            return item;
        }

        static GfxLogicalBindingItem PushConstants(UInt32 slot, UInt32 byte_size) {
            GfxLogicalBindingItem item = MakeBase(cutie::BindingSetItem::PushConstants(slot, byte_size));
            return item;
        }
    };

    struct GfxLogicalBindingSetDesc {
        DynamicArray<GfxLogicalBindingItem> bindings{};

        GfxLogicalBindingSetDesc& addItem(const GfxLogicalBindingItem& item) {
            bindings.push_back(item);
            return *this;
        }

        [[nodiscard]] Size_t size() const { return bindings.size(); }
    };

    struct GfxVertexBindingDesc {
        GfxBufferHandle buffer{};
        UInt32 slot{0};
        UInt64 offset{0};

        [[nodiscard]] Bool operator==(const GfxVertexBindingDesc& other) const {
            return buffer.get() == other.buffer.get() && slot == other.slot && offset == other.offset;
        }
        [[nodiscard]] Bool operator!=(const GfxVertexBindingDesc& other) const { return !(*this == other); }

        GfxVertexBindingDesc& setBuffer(const GfxBufferHandle& value) { buffer = value; return *this; }
        GfxVertexBindingDesc& setSlot(UInt32 value) { slot = value; return *this; }
        GfxVertexBindingDesc& setOffset(UInt64 value) { offset = value; return *this; }
    };

    struct GfxIndexBindingDesc {
        GfxBufferHandle buffer{};
        GfxFormat format{GfxFormat::UNKNOWN};
        UInt32 offset{0};

        [[nodiscard]] Bool operator==(const GfxIndexBindingDesc& other) const {
            return buffer.get() == other.buffer.get() && format == other.format && offset == other.offset;
        }
        [[nodiscard]] Bool operator!=(const GfxIndexBindingDesc& other) const { return !(*this == other); }

        GfxIndexBindingDesc& setBuffer(const GfxBufferHandle& value) { buffer = value; return *this; }
        GfxIndexBindingDesc& setFormat(GfxFormat value) { format = value; return *this; }
        GfxIndexBindingDesc& setOffset(UInt32 value) { offset = value; return *this; }
    };

    struct GfxDrawState {
        GfxGraphicsPipelineHandle pipeline{};
        GfxFramebufferHandle framebuffer{};
        GfxViewportState viewport{};
        GfxColor blend_constant_color{};
        UInt8 dynamic_stencil_ref_value{0};
        DynamicArray<GfxBindingSetHandle> bindings{};
        DynamicArray<GfxVertexBindingDesc> vertex_buffers{};
        GfxIndexBindingDesc index_buffer{};
        GfxBufferHandle indirect_params{};
        GfxBufferHandle indirect_count_buffer{};

        GfxDrawState& setPipeline(const GfxGraphicsPipelineHandle& value) { pipeline = value; return *this; }
        GfxDrawState& setFramebuffer(const GfxFramebufferHandle& value) { framebuffer = value; return *this; }
        GfxDrawState& setViewport(const GfxViewportState& value) { viewport = value; return *this; }
        GfxDrawState& setBlendColor(const GfxColor& value) { blend_constant_color = value; return *this; }
        GfxDrawState& setDynamicStencilRefValue(UInt8 value) { dynamic_stencil_ref_value = value; return *this; }
        GfxDrawState& addBindingSet(const GfxBindingSetHandle& value) { bindings.push_back(value); return *this; }
        GfxDrawState& addVertexBuffer(const GfxVertexBindingDesc& value) { vertex_buffers.push_back(value); return *this; }
        GfxDrawState& setIndexBuffer(const GfxIndexBindingDesc& value) { index_buffer = value; return *this; }
        GfxDrawState& setIndirectParams(const GfxBufferHandle& value) { indirect_params = value; return *this; }
        GfxDrawState& setIndirectCountBuffer(const GfxBufferHandle& value) { indirect_count_buffer = value; return *this; }
    };

    struct GfxComputeDispatchState {
        GfxComputePipelineHandle pipeline{};
        DynamicArray<GfxBindingSetHandle> bindings{};
        GfxBufferHandle indirect_params{};

        GfxComputeDispatchState& setPipeline(GfxComputePipelineHandle value) { pipeline = value; return *this; }
        GfxComputeDispatchState& addBindingSet(const GfxBindingSetHandle& value) { bindings.push_back(value); return *this; }
        GfxComputeDispatchState& setIndirectParams(const GfxBufferHandle& value) { indirect_params = value; return *this; }
    };

    class GfxBindingSet {
        cutie::BindingSetHandle m_rhi{};
        GfxLogicalBindingSetDesc m_logical_desc{};
        GfxBindingLayoutHandle m_layout{};
        std::atomic<bool> m_gpu_ready{false};
    public:
        GfxBindingSet() = default;
        void setPendingDesc(const GfxLogicalBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
            if (!m_gpu_ready) {
                m_logical_desc = desc;
                m_layout = layout;
            }
        }
        void initializeGpu(GfxDeviceHandle device) {
            if (!m_gpu_ready) {
                cutie::BindingSetDesc desc{};
                for (const auto& item : m_logical_desc.bindings) {
                    desc.addItem(item.toRHI());
                }
                m_rhi = device->createBindingSet(desc, m_layout);
                m_gpu_ready = m_rhi != nullptr;
            }
        }
        void initializeGpu(GfxDeviceHandle device, const GfxLogicalBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
            setPendingDesc(desc, layout);
            initializeGpu(device);
        }
        explicit GfxBindingSet(const cutie::BindingSetHandle& handle) : m_rhi(handle), m_gpu_ready(true) {}
        [[nodiscard]] cutie::IBindingSet* getRHI() const { return m_rhi.Get(); }
        [[nodiscard]] const cutie::BindingSetHandle& getRHIHandle() const { return m_rhi; }
        [[nodiscard]] const GfxLogicalBindingSetDesc& getLogicalDesc() const { return m_logical_desc; }
        [[nodiscard]] const GfxBindingLayoutHandle& getLayout() const { return m_layout; }
        [[nodiscard]] bool isGpuReady() const { return m_gpu_ready.load(std::memory_order_acquire); }
    };

} // dodoe
