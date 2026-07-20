// do@Redlive

#include "draw_command_list.h"
#include "gfx_context.h"

namespace dodoe {

    DrawCommandList GDrawCommandList;

    void DrawCommandList::setDevice(GfxContext& gfx) {
        m_device = gfx.getDevice();
        m_resource_mode = true;
    }

    void DrawCommandList::beginFrame() { reset(); }
    void DrawCommandList::endFrame() {}

    void DrawCommandList::execute(const GfxCommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        CommandList::execute(*command_list);
    }

    void DrawCommandList::open() {
        DO_ASSERT(!m_resource_mode, "open() not allowed in resource mode");
        enqueue<OpenCommand>();
    }
    void DrawCommandList::close() {
        DO_ASSERT(!m_resource_mode, "close() not allowed in resource mode");
        enqueue<CloseCommand>();
    }
    void DrawCommandList::clearState() {
        DO_ASSERT(!m_resource_mode, "clearState() not allowed in resource mode");
        enqueue<ClearStateCommand>();
    }

    void DrawCommandList::beginMarker(const char* name) {
        DO_ASSERT(!m_resource_mode, "beginMarker() not allowed in resource mode");
        BeginMarkerCommand::Create(*this, name);
    }
    void DrawCommandList::endMarker() {
        DO_ASSERT(!m_resource_mode, "endMarker() not allowed in resource mode");
        enqueue<EndMarkerCommand>();
    }

    void DrawCommandList::clearTextureFloat(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color) {
        DO_ASSERT(!m_resource_mode, "clearTextureFloat() not allowed in resource mode");
        enqueue<ClearTextureFloatCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearTextureUInt(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, UInt32 clear_color) {
        DO_ASSERT(!m_resource_mode, "clearTextureUInt() not allowed in resource mode");
        enqueue<ClearTextureUIntCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearDepthStencilTexture(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, Bool clear_depth, Float depth, Bool clear_stencil, UInt8 stencil) {
        DO_ASSERT(!m_resource_mode, "clearDepthStencilTexture() not allowed in resource mode");
        enqueue<ClearDepthStencilTextureCommand>(texture, subresources, clear_depth, depth, clear_stencil, stencil);
    }
    void DrawCommandList::copyBuffer(const GfxBufferHandle& destination, UInt64 destination_offset_bytes, const GfxBufferHandle& source, UInt64 source_offset_bytes, UInt64 data_size_bytes) {
        DO_ASSERT(!m_resource_mode, "copyBuffer() not allowed in resource mode");
        enqueue<CopyBufferCommand>(destination, destination_offset_bytes, source, source_offset_bytes, data_size_bytes);
    }

    void DrawCommandList::writeBuffer(const GfxBufferHandle& buffer, const void* data, Size_t data_size, UInt64 destination_offset_bytes) {
        if (m_resource_mode) {
            auto cmd = m_device->createCommandList();
            cmd->open();
            cmd->writeBuffer(buffer->getRHIHandle(), data, data_size, destination_offset_bytes);
            cmd->close();
            m_device->executeCommandList(cmd);
            return;
        }
        WriteBufferCommand::Create(*this, data_size, data, buffer, destination_offset_bytes);
    }
    void DrawCommandList::writeTexture(const GfxTextureHandle& texture, UInt32 mip_level, UInt32 array_slice, const void* data, Size_t row_pitch) {
        DO_ASSERT(texture != nullptr, "writeTexture: texture is null");
        if (m_resource_mode) {
            const Size_t data_size = static_cast<Size_t>(texture->getHeight()) * row_pitch;
            auto cmd = m_device->createCommandList();
            cmd->open();
            cmd->writeTexture(texture->getRHIHandle(), mip_level, array_slice, data, row_pitch);
            cmd->close();
            m_device->executeCommandList(cmd);
            return;
        }
        const Size_t data_size = static_cast<Size_t>(texture->getHeight()) * row_pitch;
        WriteTextureCommand::Create(*this, data_size, data, texture, mip_level, array_slice, row_pitch);
    }
    void DrawCommandList::setPushConstants(const void* data, Size_t byte_size) {
        DO_ASSERT(!m_resource_mode, "setPushConstants() not allowed in resource mode");
        PushConstantsCommand::Create(*this, byte_size, data);
    }

    void DrawCommandList::setTextureState(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, GfxResourceStates state_bits) {
        DO_ASSERT(!m_resource_mode, "setTextureState() not allowed in resource mode");
        enqueue<SetTextureStateCommand>(texture, subresources, state_bits);
    }
    void DrawCommandList::setBufferState(const GfxBufferHandle& buffer, GfxResourceStates state_bits) {
        DO_ASSERT(!m_resource_mode, "setBufferState() not allowed in resource mode");
        enqueue<SetBufferStateCommand>(buffer, state_bits);
    }
    void DrawCommandList::commitBarriers() {
        DO_ASSERT(!m_resource_mode, "commitBarriers() not allowed in resource mode");
        enqueue<CommitBarriersCommand>();
    }

    void DrawCommandList::setGraphicsState(const GfxFramebufferHandle& framebuffer, const GfxGraphicsPipelineHandle& pipeline, const DynamicArray<GfxBindingSetHandle>& binding_sets, const GfxViewportState& viewport, const DynamicArray<GfxVertexBufferBinding>& vertex_buffers, const GfxIndexBufferBinding& index_buffer) {
        DO_ASSERT(!m_resource_mode, "setGraphicsState() not allowed in resource mode");
        enqueue<SetGraphicsStateCommand>(framebuffer, pipeline, binding_sets, viewport, vertex_buffers, index_buffer);
    }
    void DrawCommandList::setGraphicsState(const GfxGraphicsState& state) {
        DO_ASSERT(!m_resource_mode, "setGraphicsState() not allowed in resource mode");
        enqueue<SetGraphicsStateByValueCommand>(state);
    }
    void DrawCommandList::setComputeState(const GfxComputeState& state) {
        DO_ASSERT(!m_resource_mode, "setComputeState() not allowed in resource mode");
        enqueue<SetComputeStateCommand>(state);
    }

    void DrawCommandList::draw(const GfxDrawArguments& args) { DO_ASSERT(!m_resource_mode, "draw() not allowed in resource mode"); enqueue<DrawPrimitiveCommand>(args); }
    void DrawCommandList::drawIndexed(const GfxDrawArguments& args) { DO_ASSERT(!m_resource_mode, "drawIndexed() not allowed in resource mode"); enqueue<DrawIndexedPrimitiveCommand>(args); }
    void DrawCommandList::drawIndirect(UInt32 offset_bytes, UInt32 draw_count) { DO_ASSERT(!m_resource_mode, "drawIndirect() not allowed in resource mode"); enqueue<DrawIndirectCommand>(offset_bytes, draw_count); }
    void DrawCommandList::drawIndexedIndirect(UInt32 offset_bytes, UInt32 draw_count) { DO_ASSERT(!m_resource_mode, "drawIndexedIndirect() not allowed in resource mode"); enqueue<DrawIndexedIndirectCommand>(offset_bytes, draw_count); }
    void DrawCommandList::dispatch(UInt32 x, UInt32 y, UInt32 z) { DO_ASSERT(!m_resource_mode, "dispatch() not allowed in resource mode"); enqueue<DispatchCommand>(x, y, z); }
    void DrawCommandList::dispatchIndirect(UInt32 offset_bytes) { DO_ASSERT(!m_resource_mode, "dispatchIndirect() not allowed in resource mode"); enqueue<DispatchIndirectCommand>(offset_bytes); }

    GfxTextureHandle DrawCommandList::createTexture(const GfxTextureDesc& desc, const void* data, Size_t data_size) {
        DO_ASSERT(m_resource_mode, "createTexture() only allowed in resource mode");
        auto texture = create_ref<GfxTexture>(desc);
        texture->initializeRHI(m_device);
        if (data && data_size > 0) {
            const UInt32 bpp = desc.format == GfxFormat::RGBA32_FLOAT ? 16u : 4u;
            const Size_t row_pitch = static_cast<Size_t>(desc.width) * bpp;
            auto cmd = m_device->createCommandList();
            cmd->open();
            cmd->writeTexture(texture->getRHIHandle(), 0, 0, data, row_pitch);
            cmd->close();
            m_device->executeCommandList(cmd);
        }
        return texture;
    }
    GfxBufferHandle DrawCommandList::createBuffer(const GfxBufferDesc& desc, const void* data, Size_t data_size) {
        DO_ASSERT(m_resource_mode, "createBuffer() only allowed in resource mode");
        auto buffer = create_ref<GfxBuffer>(desc);
        buffer->initializeRHI(m_device);
        if (data && data_size > 0) {
            auto cmd = m_device->createCommandList();
            cmd->open();
            cmd->writeBuffer(buffer->getRHIHandle(), data, data_size, 0);
            cmd->close();
            m_device->executeCommandList(cmd);
        }
        return buffer;
    }
    GfxFramebufferHandle DrawCommandList::createFramebuffer(const GfxFramebufferDesc& desc) {
        DO_ASSERT(m_resource_mode, "createFramebuffer() only allowed in resource mode");
        auto fb = create_ref<GfxFramebuffer>(desc);
        fb->initializeRHI(m_device);
        return fb;
    }
    GfxBindingSetHandle DrawCommandList::createBindingSet(const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
        DO_ASSERT(m_resource_mode, "createBindingSet() only allowed in resource mode");
        auto bs = create_ref<GfxBindingSet>();
        bs->initializeRHI(m_device, desc, layout);
        return bs;
    }
    GfxGraphicsPipelineHandle DrawCommandList::createGraphicsPipeline(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
        DO_ASSERT(m_resource_mode, "createGraphicsPipeline() only allowed in resource mode");
        auto pso = create_ref<GfxGraphicsPipeline>();
        pso->initializeRHI(m_device, desc, info);
        return pso;
    }
    DescriptorIndex DrawCommandList::createDescriptor(GfxDescriptorTableHandle table, GfxTextureHandle texture, UInt32 slot) {
        DO_ASSERT(m_resource_mode, "createDescriptor() only allowed in resource mode");
        if (texture && texture->isRHIReady()) {
            auto item = GfxBindingSetItem::Texture_SRV(0, texture->getRHIHandle());
            item.slot = slot;
            m_device->writeDescriptorTable(table, item);
        }
        return 0;
    }
    GfxSamplerHandle DrawCommandList::createSampler(const GfxSamplerDesc& desc) { return m_device->createSampler(desc); }
    GfxBindingLayoutHandle DrawCommandList::createBindingLayout(const GfxBindingLayoutDesc& desc) { return m_device->createBindingLayout(desc); }
    GfxInputLayoutHandle DrawCommandList::createInputLayout(const GfxVertexAttributeDesc* a, UInt32 c, GfxShaderHandle sh) { return m_device->createInputLayout(a, c, sh); }
    GfxShaderHandle DrawCommandList::createShader(const GfxShaderDesc& desc, const void* data, Size_t data_size) {
        DO_ASSERT(m_resource_mode, "createShader() only allowed in resource mode");
        return m_device->createShader(desc, data, data_size);
    }

    void DrawCommandList::OpenCommand::execute(GfxCommandList& c) const { c.open(); }
    void DrawCommandList::CloseCommand::execute(GfxCommandList& c) const { c.close(); }
    void DrawCommandList::ClearStateCommand::execute(GfxCommandList& c) const { c.clearState(); }
    void DrawCommandList::EndMarkerCommand::execute(GfxCommandList& c) const { c.endMarker(); }
    void DrawCommandList::CommitBarriersCommand::execute(GfxCommandList& c) const { c.commitBarriers(); }
    void DrawCommandList::SetGraphicsStateByValueCommand::execute(GfxCommandList& c) const { c.setGraphicsState(m_st); }
    void DrawCommandList::SetComputeStateCommand::execute(GfxCommandList& c) const { c.setComputeState(m_st); }
    void DrawCommandList::DrawPrimitiveCommand::execute(GfxCommandList& c) const { c.draw(m_args); }
    void DrawCommandList::DrawIndexedPrimitiveCommand::execute(GfxCommandList& c) const { c.drawIndexed(m_args); }
    void DrawCommandList::DispatchCommand::execute(GfxCommandList& c) const { c.dispatch(m_x, m_y, m_z); }
    void DrawCommandList::DrawIndirectCommand::execute(GfxCommandList& c) const { c.drawIndirect(m_off, m_cnt); }
    void DrawCommandList::DrawIndexedIndirectCommand::execute(GfxCommandList& c) const { c.drawIndexedIndirect(m_off, m_cnt); }
    void DrawCommandList::DispatchIndirectCommand::execute(GfxCommandList& c) const { c.dispatchIndirect(m_off); }

    void DrawCommandList::ClearTextureFloatCommand::execute(GfxCommandList& c) const { c.clearTextureFloat(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearTextureUIntCommand::execute(GfxCommandList& c) const { c.clearTextureUInt(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearDepthStencilTextureCommand::execute(GfxCommandList& c) const { c.clearDepthStencilTexture(m_texture->getRHIHandle(), m_subresources, m_clear_depth, m_depth, m_clear_stencil, m_stencil); }
    void DrawCommandList::CopyBufferCommand::execute(GfxCommandList& c) const { c.copyBuffer(m_dst->getRHIHandle(), m_dst_off, m_src->getRHIHandle(), m_src_off, m_size); }
    void DrawCommandList::SetTextureStateCommand::execute(GfxCommandList& c) const { c.setTextureState(m_t->getRHIHandle(), m_s, m_st); }
    void DrawCommandList::SetBufferStateCommand::execute(GfxCommandList& c) const { c.setBufferState(m_b->getRHIHandle(), m_st); }

    void DrawCommandList::SetGraphicsStateCommand::execute(GfxCommandList& cm) const {
        GfxGraphicsState s;
        s.setViewport(m_vp);
        if (m_pso && m_pso->isRHIReady()) s.setPipeline(m_pso->getRHIHandle());
        if (m_fb && m_fb->isRHIReady()) s.setFramebuffer(m_fb->getRHIHandle());
        for (auto& bs : m_bs) { if (bs && bs->isRHIReady()) s.addBindingSet(bs->getRHIHandle()); }
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
