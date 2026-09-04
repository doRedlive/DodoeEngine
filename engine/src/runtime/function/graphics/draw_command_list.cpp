// do@Redlive

#include "draw_command_list.h"
#include "gfx_context.h"

namespace dodoe {

    DrawCommandList GDrawCommandList;

    namespace {

        Bool IsFramebufferAttachmentsReady(const GfxFramebufferDesc& desc) {
            for (const auto& color : desc.colors()) {
                if (!color || !color->isGpuReady()) return false;
            }
            const GfxTextureHandle& depth = desc.depth();
            if (depth && !depth->isGpuReady()) return false;
            return true;
        }

        Bool IsBindingSetResourcesReady(const GfxBindingSetDesc& desc) {
            for (const auto& item : desc.bindings) {
                switch (item.type) {
                case cutie::ResourceType::Texture_SRV:
                case cutie::ResourceType::Texture_UAV:
                case cutie::ResourceType::TypedBuffer_SRV:
                case cutie::ResourceType::TypedBuffer_UAV:
                case cutie::ResourceType::StructuredBuffer_SRV:
                case cutie::ResourceType::StructuredBuffer_UAV:
                case cutie::ResourceType::RawBuffer_SRV:
                case cutie::ResourceType::RawBuffer_UAV:
                case cutie::ResourceType::ConstantBuffer:
                case cutie::ResourceType::VolatileConstantBuffer:
                    if (item.resourceHandle == nullptr) return false;
                    break;
                default:
                    break;
                }
            }
            return true;
        }

    } // namesapce

    void DrawCommandList::setDevice(GfxContext& gfx) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::setDevice", "startup");
        m_device = gfx.getDevice();
        if (!m_device) {
            DO_ERROR("DrawCommandList::setDevice: graphics device is unavailable");
        }
    }

    void DrawCommandList::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::shutdown", "shutdown");
        m_device = nullptr;
    }

    void DrawCommandList::beginFrame() { reset(); }
    void DrawCommandList::endFrame() {}

    CommandList<GfxCommandList> DrawCommandList::detachRecordedCommands() {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        return CommandList<GfxCommandList>(std::move(*this));
    }

    void DrawCommandList::execute(const GfxCommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        CommandList::execute(*command_list);
    }

    void DrawCommandList::open()  { recordCommand<OpenCommand>(); }
    void DrawCommandList::close() { recordCommand<CloseCommand>(); }
    void DrawCommandList::clearState() { recordCommand<ClearStateCommand>(); }

    void DrawCommandList::beginMarker(const char* name) {
        BeginMarkerCommand::Create(*this, name);
    }
    void DrawCommandList::endMarker() { recordCommand<EndMarkerCommand>(); }

    void DrawCommandList::clearTextureFloat(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color) {
        recordCommand<ClearTextureFloatCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearTextureUInt(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, UInt32 clear_color) {
        recordCommand<ClearTextureUIntCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearDepthStencilTexture(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, Bool clear_depth, Float depth, Bool clear_stencil, UInt8 stencil) {
        recordCommand<ClearDepthStencilTextureCommand>(texture, subresources, clear_depth, depth, clear_stencil, stencil);
    }
    void DrawCommandList::copyBuffer(const GfxBufferHandle& destination, UInt64 destination_offset_bytes, const GfxBufferHandle& source, UInt64 source_offset_bytes, UInt64 data_size_bytes) {
        recordCommand<CopyBufferCommand>(destination, destination_offset_bytes, source, source_offset_bytes, data_size_bytes);
    }

    void DrawCommandList::writeBuffer(const GfxBufferHandle& buffer, const void* data, Size_t data_size, UInt64 destination_offset_bytes) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::writeBuffer", "resource-upload");
        if (!buffer || !m_device) {
            DO_ERROR("DrawCommandList::writeBuffer: invalid buffer or device");
            return;
        }
        if (data_size == 0) {
            return;
        }
        if (!data) {
            DO_ERROR("DrawCommandList::writeBuffer: null data with size {}", data_size);
            return;
        }
        if (!buffer->isGpuReady()) {
            DO_WARN("DrawCommandList::writeBuffer: buffer not realized, deferring upload");
        }
        WriteBufferCommand::Create(*this, buffer, data, data_size, destination_offset_bytes);
    }
    void DrawCommandList::writeTexture(const GfxTextureHandle& texture, UInt32 mip_level, UInt32 array_slice, const void* data, Size_t row_pitch) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::writeTexture", "resource-upload");
        DO_ASSERT(texture != nullptr, "writeTexture: texture is null");
        if (!texture || !m_device || !data || row_pitch == 0) {
            DO_ERROR("DrawCommandList::writeTexture: invalid texture, device, data, or row pitch");
            return;
        }
        if (!texture->isGpuReady()) {
            DO_WARN("DrawCommandList::writeTexture: texture not realized, deferring upload");
        }
        const Size_t data_size = static_cast<Size_t>(texture->getHeight()) * row_pitch;
        WriteTextureCommand::Create(*this, texture, mip_level, array_slice, data, row_pitch, data_size);
    }

    void DrawCommandList::setPushConstants(const void* data, Size_t byte_size) {
        PushConstantsCommand::Create(*this, data, byte_size);
    }

    void DrawCommandList::setTextureState(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, GfxResourceStates state_bits) {
        recordCommand<SetTextureStateCommand>(texture, subresources, state_bits);
    }
    void DrawCommandList::setBufferState(const GfxBufferHandle& buffer, GfxResourceStates state_bits) {
        recordCommand<SetBufferStateCommand>(buffer, state_bits);
    }
    void DrawCommandList::commitBarriers() {
        recordCommand<CommitBarriersCommand>();
    }

    void DrawCommandList::setGraphicsState(const GfxFramebufferHandle& framebuffer, const GfxGraphicsPipelineHandle& pipeline, const DynamicArray<GfxBindingSetHandle>& binding_sets, const GfxViewportState& viewport, const DynamicArray<GfxVertexBufferBinding>& vertex_buffers, const GfxIndexBufferBinding& index_buffer) {
        recordCommand<SetGraphicsStateCommand>(framebuffer, pipeline, binding_sets, viewport, vertex_buffers, index_buffer);
    }
    void DrawCommandList::setGraphicsState(const GfxGraphicsState& state) {
        recordCommand<SetGraphicsStateByValueCommand>(state);
    }
    void DrawCommandList::setComputeState(const GfxComputeState& state) {
        recordCommand<SetComputeStateCommand>(state);
    }

    void DrawCommandList::draw(const GfxDrawArguments& args) {
        recordCommand<DrawPrimitiveCommand>(args);
    }
    void DrawCommandList::drawIndexed(const GfxDrawArguments& args) {
        recordCommand<DrawIndexedPrimitiveCommand>(args);
    }
    void DrawCommandList::drawIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        recordCommand<DrawIndirectCommand>(offset_bytes, draw_count);
    }
    void DrawCommandList::drawIndexedIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        recordCommand<DrawIndexedIndirectCommand>(offset_bytes, draw_count);
    }
    void DrawCommandList::dispatch(UInt32 x, UInt32 y, UInt32 z) {
        recordCommand<DispatchCommand>(x, y, z);
    }
    void DrawCommandList::dispatchIndirect(UInt32 offset_bytes) {
        recordCommand<DispatchIndirectCommand>(offset_bytes);
    }

    GfxTextureHandle DrawCommandList::createTexture(const GfxTextureDesc& desc, const void* data, Size_t data_size) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createTexture", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createTexture: graphics device is unavailable");
            return nullptr;
        }
        auto texture = create_ref<GfxTexture>(desc);
        texture->initializeGpu(m_device);
        if (data && data_size > 0) {
            const UInt32 bpp = desc.format == GfxFormat::RGBA32_FLOAT ? 16u : 4u;
            const UInt32 pitch = desc.width * bpp;
            writeTexture(texture, 0, 0, data, pitch);
        }
        // DO_DEBUG("DrawCommandList: created texture ({}x{}, data={})", desc.width, desc.height, data_size > 0);
        return texture;
    }
    GfxBufferHandle DrawCommandList::createBuffer(const GfxBufferDesc& desc, const void* data, Size_t data_size) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createBuffer", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createBuffer: graphics device is unavailable");
            return nullptr;
        }
        auto buffer = create_ref<GfxBuffer>(desc);
        buffer->initializeGpu(m_device);
        if (data && data_size > 0) {
            writeBuffer(buffer, data, data_size, 0);
        }
        // DO_DEBUG("DrawCommandList: created buffer (size={}, data={})", desc.byteSize, data_size > 0);
        return buffer;
    }
    GfxFramebufferHandle DrawCommandList::createFramebuffer(const GfxFramebufferDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createFramebuffer", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createFramebuffer: graphics device is unavailable");
            return nullptr;
        }
        auto fb = create_ref<GfxFramebuffer>(desc);
        if (IsFramebufferAttachmentsReady(desc)) {
            fb->initializeGpu(m_device);
        } else {
            // DO_DEBUG("DrawCommandList: deferred framebuffer creation because attachments are not ready");
        }
        return fb;
    }
    GfxBindingSetHandle DrawCommandList::createBindingSet(const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createBindingSet", "resource");
        if (!m_device || !layout) {
            DO_ERROR("DrawCommandList::createBindingSet: graphics device or layout is unavailable");
            return nullptr;
        }
        auto bs = create_ref<GfxBindingSet>();
        if (IsBindingSetResourcesReady(desc)) {
            bs->initializeGpu(m_device, desc, layout);
        } else {
            // DO_DEBUG("DrawCommandList: deferred binding set creation because resources are not ready");
        }
        return bs;
    }
    GfxGraphicsPipelineHandle DrawCommandList::createGraphicsPipeline(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createGraphicsPipeline", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createGraphicsPipeline: graphics device is unavailable");
            return nullptr;
        }
        auto pso = create_ref<GfxGraphicsPipeline>();
        pso->initializeGpu(m_device, desc, info);
        return pso;
    }
    DescriptorIndex DrawCommandList::createDescriptor(GfxDescriptorTableHandle table, GfxTextureHandle texture, UInt32 slot) {
        if (texture && texture->isGpuReady()) {
            auto item = GfxBindingSetItem::Texture_SRV(0, texture->getRHIHandle());
            item.slot = slot;
            m_device->writeDescriptorTable(table, item);
        }
        return 0;
    }
    GfxSamplerHandle DrawCommandList::createSampler(const GfxSamplerDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createSampler", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createSampler: graphics device is unavailable");
            return nullptr;
        }
        return m_device->createSampler(desc);
    }
    GfxBindingLayoutHandle DrawCommandList::createBindingLayout(const GfxBindingLayoutDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createBindingLayout", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createBindingLayout: graphics device is unavailable");
            return nullptr;
        }
        return m_device->createBindingLayout(desc);
    }
    GfxInputLayoutHandle DrawCommandList::createInputLayout(const GfxVertexAttributeDesc* a, UInt32 c, GfxShaderHandle sh) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createInputLayout", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createInputLayout: graphics device is unavailable");
            return nullptr;
        }
        return m_device->createInputLayout(a, c, sh);
    }
    GfxShaderHandle DrawCommandList::createShader(const GfxShaderDesc& desc, const void* data, Size_t data_size) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createShader", "shader");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createShader: graphics device is unavailable");
            return nullptr;
        }
        auto shader = m_device->createShader(desc, data, data_size);
        if (!shader) {
            DO_ERROR("DrawCommandList::createShader: device failed to create shader");
        }
        return shader;
    }

    void DrawCommandList::OpenCommand::execute(GfxCommandList& c) const { c.open(); }
    void DrawCommandList::CloseCommand::execute(GfxCommandList& c) const { c.close(); }
    void DrawCommandList::ClearStateCommand::execute(GfxCommandList& c) const { c.clearState(); }
    void DrawCommandList::EndMarkerCommand::execute(GfxCommandList& c) const { c.endMarker(); }
    void DrawCommandList::CommitBarriersCommand::execute(GfxCommandList& c) const { c.commitBarriers(); }
    void DrawCommandList::SetComputeStateCommand::execute(GfxCommandList& c) const { c.setComputeState(m_st); }
    void DrawCommandList::DrawPrimitiveCommand::execute(GfxCommandList& c) const { c.draw(m_args); }
    void DrawCommandList::DrawIndexedPrimitiveCommand::execute(GfxCommandList& c) const { c.drawIndexed(m_args); }
    void DrawCommandList::DispatchCommand::execute(GfxCommandList& c) const { c.dispatch(m_x, m_y, m_z); }
    void DrawCommandList::DrawIndirectCommand::execute(GfxCommandList& c) const { c.drawIndirect(m_off, m_cnt); }
    void DrawCommandList::DrawIndexedIndirectCommand::execute(GfxCommandList& c) const { c.drawIndexedIndirect(m_off, m_cnt); }
    void DrawCommandList::DispatchIndirectCommand::execute(GfxCommandList& c) const { c.dispatchIndirect(m_off); }

    void DrawCommandList::ClearTextureFloatCommand::execute(GfxCommandList& c) const { if (m_texture->isGpuReady()) c.clearTextureFloat(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearTextureUIntCommand::execute(GfxCommandList& c) const { if (m_texture->isGpuReady()) c.clearTextureUInt(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearDepthStencilTextureCommand::execute(GfxCommandList& c) const { if (m_texture->isGpuReady()) c.clearDepthStencilTexture(m_texture->getRHIHandle(), m_subresources, m_clear_depth, m_depth, m_clear_stencil, m_stencil); }
    void DrawCommandList::CopyBufferCommand::execute(GfxCommandList& c) const { if (m_dst->isGpuReady() && m_src->isGpuReady()) c.copyBuffer(m_dst->getRHIHandle(), m_dst_off, m_src->getRHIHandle(), m_src_off, m_size); }
    void DrawCommandList::SetTextureStateCommand::execute(GfxCommandList& c) const { if (m_t->isGpuReady()) c.setTextureState(m_t->getRHIHandle(), m_s, m_st); else DO_WARN("SetTextureState: texture not realized"); }
    void DrawCommandList::SetBufferStateCommand::execute(GfxCommandList& c) const { if (m_b->isGpuReady()) c.setBufferState(m_b->getRHIHandle(), m_st); else DO_WARN("SetBufferState: buffer not realized"); }

    void DrawCommandList::SetGraphicsStateCommand::execute(GfxCommandList& cm) const {
        GfxGraphicsState s;
        s.setViewport(m_vp);
        if (m_pso && m_pso->isGpuReady()) s.setPipeline(m_pso->getRHIHandle());
        if (m_fb && m_fb->isGpuReady()) s.setFramebuffer(m_fb->getRHIHandle());
        else if (m_fb) DO_WARN("SetGraphicsState: framebuffer skipped, rhi_ready={}", m_fb->isGpuReady());
        for (auto& bs : m_bs) {
            if (bs && bs->isGpuReady()) s.addBindingSet(bs->getRHIHandle());
            else if (bs) DO_WARN("SetGraphicsState: binding set skipped, rhi_ready={}", bs->isGpuReady());
        }
        for (auto& vb : m_vb) s.addVertexBuffer(vb);
        s.setIndexBuffer(m_ib);
        cm.setGraphicsState(s);
    }

    Size_t DrawCommandList::alignUp(Size_t v, Size_t a) {
        DO_ASSERT(a != 0 && (a & (a - 1)) == 0, "alignUp: invalid alignment");
        return (v + a - 1) & ~(a - 1);
    }

    DrawCommandList::SetGraphicsStateCommand::SetGraphicsStateCommand(
        const GfxFramebufferHandle& fb,
        const GfxGraphicsPipelineHandle& pso,
        const DynamicArray<GfxBindingSetHandle>& bs,
        const GfxViewportState& vp,
        const DynamicArray<GfxVertexBufferBinding>& vb,
        const GfxIndexBufferBinding& ib)
        : m_fb(fb), m_pso(pso), m_bs(bs), m_vp(vp), m_vb(vb), m_ib(ib) {}

} // dodoe
