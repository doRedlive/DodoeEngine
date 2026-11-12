// do@Redlive

#include "draw_command_list.h"
#include "gfx_context.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"

namespace dodoe {

    DrawCommandList GDrawCommandList;

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

    void DrawCommandList::beginFrame() {
        resetAndRelease();
        std::lock_guard<std::mutex> lock(m_record_mutex);
        m_realize_plan.clear();
    }
    void DrawCommandList::endFrame() {}

    DrawCommandList DrawCommandList::detachRecordedCommands() {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        DrawCommandList detached{};
        detached.m_device = m_device;
        static_cast<CommandList<GfxCommandList>&>(detached) = CommandList<GfxCommandList>(std::move(*this));
        detached.m_realize_plan = std::move(m_realize_plan);
        return detached;
    }

    void DrawCommandList::execute(const GfxCommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        CommandList::execute(*command_list);
    }

    void DrawCommandList::registerRealizeEntry(const GfxResourceRealizeEntry& entry) {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        m_realize_plan.push_back(entry);
    }

    void DrawCommandList::requestRealize(const GfxTextureHandle& texture) {
        if (!texture || texture->isGpuReady()) return;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Texture;
        entry.texture = texture;
        registerRealizeEntry(entry);
    }

    void DrawCommandList::requestRealize(const GfxBufferHandle& buffer) {
        if (!buffer || buffer->isGpuReady()) return;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Buffer;
        entry.buffer = buffer;
        registerRealizeEntry(entry);
    }

    void DrawCommandList::requestRealize(const GfxFramebufferHandle& framebuffer) {
        if (!framebuffer || framebuffer->isGpuReady()) return;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Framebuffer;
        entry.framebuffer = framebuffer;
        registerRealizeEntry(entry);
    }

    void DrawCommandList::requestRealize(const GfxBindingSetHandle& binding_set) {
        if (!binding_set || binding_set->isGpuReady()) return;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::BindingSet;
        entry.binding_set = binding_set;
        registerRealizeEntry(entry);
    }

    void DrawCommandList::requestRealize(const GfxGraphicsPipelineHandle& pipeline) {
        if (!pipeline || pipeline->isGpuReady()) return;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::GraphicsPipeline;
        entry.pipeline = pipeline;
        registerRealizeEntry(entry);
    }

    void DrawCommandList::append(DrawCommandList&& other) {
        if (!other.m_realize_plan.empty()) {
            for (auto& entry : other.m_realize_plan) {
                m_realize_plan.push_back(std::move(entry));
            }
            other.m_realize_plan.clear();
        }
        CommandList<GfxCommandList>::append(std::move(other));
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
    void DrawCommandList::copyTextureToStaging(const GfxStagingTextureHandle& destination, const GfxTextureSlice& destination_slice, const GfxTextureHandle& source, const GfxTextureSlice& source_slice) {
        recordCommand<CopyTextureToStagingCommand>(destination, destination_slice, source, source_slice);
    }

    void DrawCommandList::resolveTexture(const GfxTextureHandle& destination, const GfxTextureHandle& source, const GfxTextureSubresourceSet& subresources) {
        recordCommand<ResolveTextureCommand>(destination, source, subresources);
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
            DO_DEBUG("DrawCommandList::writeBuffer: buffer pending realize, upload replayed after prepare");
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
            DO_DEBUG("DrawCommandList::writeTexture: texture pending realize, upload replayed after prepare");
        }
        const auto& desc = texture->getDesc();
        UInt32 effective_mip_height = desc.height >> mip_level;
        if (effective_mip_height == 0) {
            effective_mip_height = 1;
        }
        const Size_t data_size = static_cast<Size_t>(effective_mip_height) * row_pitch;
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
        RenderFrameCounters::Self().addBarrier();
        recordCommand<CommitBarriersCommand>();
    }

    void DrawCommandList::setGraphicsState(const GfxFramebufferHandle& framebuffer, const GfxGraphicsPipelineHandle& pipeline, const DynamicArray<GfxBindingSetHandle>& binding_sets, const GfxViewportState& viewport, const DynamicArray<GfxVertexBindingDesc>& vertex_buffers, const GfxIndexBindingDesc& index_buffer, const GfxBufferHandle& indirect_params) {
        GfxDrawState state{};
        state.setFramebuffer(framebuffer)
            .setPipeline(pipeline)
            .setViewport(viewport)
            .setIndexBuffer(index_buffer)
            .setIndirectParams(indirect_params);
        for (const auto& binding_set : binding_sets) {
            state.addBindingSet(binding_set);
        }
        for (const auto& vertex_buffer : vertex_buffers) {
            state.addVertexBuffer(vertex_buffer);
        }
        recordCommand<SetGraphicsStateCommand>(state);
    }
    void DrawCommandList::setGraphicsState(const GfxDrawState& state) {
        recordCommand<SetGraphicsStateCommand>(state);
    }
    void DrawCommandList::setComputeState(const GfxComputeDispatchState& state) {
        recordCommand<SetComputeStateCommand>(state);
    }

    void DrawCommandList::draw(const GfxDrawArguments& args) {
        RenderFrameCounters::Self().addDrawCall(args.instanceCount);
        recordCommand<DrawPrimitiveCommand>(args);
    }
    void DrawCommandList::drawIndexed(const GfxDrawArguments& args) {
        RenderFrameCounters::Self().addDrawCall(args.instanceCount);
        recordCommand<DrawIndexedPrimitiveCommand>(args);
    }
    void DrawCommandList::drawIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        RenderFrameCounters::Self().addIndirectDrawCall(draw_count);
        recordCommand<DrawIndirectCommand>(offset_bytes, draw_count);
    }
    void DrawCommandList::drawIndexedIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        RenderFrameCounters::Self().addIndirectDrawCall(draw_count);
        recordCommand<DrawIndexedIndirectCommand>(offset_bytes, draw_count);
    }
    void DrawCommandList::dispatch(UInt32 x, UInt32 y, UInt32 z) {
        RenderFrameCounters::Self().addDispatch();
        recordCommand<DispatchCommand>(x, y, z);
    }
    void DrawCommandList::dispatchIndirect(UInt32 offset_bytes) {
        RenderFrameCounters::Self().addDispatch();
        recordCommand<DispatchIndirectCommand>(offset_bytes);
    }

    GfxTextureHandle DrawCommandList::createTexture(const GfxTextureDesc& desc, const void* data, Size_t data_size) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createTexture", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createTexture: graphics device is unavailable");
            return nullptr;
        }
        auto texture = create_ref<GfxTexture>(desc);
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Texture;
        entry.texture = texture;
        registerRealizeEntry(entry);
        if (data && data_size > 0) {
            const UInt32 bpp = desc.format == GfxFormat::RGBA32_FLOAT ? 16u : 4u;
            const UInt32 pitch = desc.width * bpp;
            writeTexture(texture, 0, 0, data, pitch);
        }
        // DO_DEBUG("DrawCommandList: registered texture ({}x{}, data={})", desc.width, desc.height, data_size > 0);
        return texture;
    }
    GfxStagingTextureHandle DrawCommandList::createStagingTexture(const GfxTextureDesc& desc, GfxCpuAccessMode cpu_access) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createStagingTexture", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createStagingTexture: graphics device is unavailable");
            return nullptr;
        }
        return m_device->createStagingTexture(desc, cpu_access);
    }

    void* DrawCommandList::mapStagingTexture(const GfxStagingTextureHandle& texture, const GfxTextureSlice& slice, GfxCpuAccessMode cpu_access, Size_t* out_row_pitch) {
        if (!m_device || !texture) {
            DO_ERROR("DrawCommandList::mapStagingTexture: device or staging texture is unavailable");
            return nullptr;
        }
        return m_device->mapStagingTexture(texture.Get(), slice, cpu_access, out_row_pitch);
    }

    void DrawCommandList::unmapStagingTexture(const GfxStagingTextureHandle& texture) {
        if (m_device && texture) {
            m_device->unmapStagingTexture(texture.Get());
        }
    }

    GfxBufferHandle DrawCommandList::createBuffer(const GfxBufferDesc& desc, const void* data, Size_t data_size) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createBuffer", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createBuffer: graphics device is unavailable");
            return nullptr;
        }
        auto buffer = create_ref<GfxBuffer>(desc);
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Buffer;
        entry.buffer = buffer;
        registerRealizeEntry(entry);
        if (data && data_size > 0) {
            writeBuffer(buffer, data, data_size, 0);
        }
        // DO_DEBUG("DrawCommandList: registered buffer (size={}, data={})", desc.byteSize, data_size > 0);
        return buffer;
    }
    GfxFramebufferHandle DrawCommandList::createFramebuffer(const GfxFramebufferDesc& desc) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createFramebuffer", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createFramebuffer: graphics device is unavailable");
            return nullptr;
        }
        auto fb = create_ref<GfxFramebuffer>(desc);
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::Framebuffer;
        entry.framebuffer = fb;
        registerRealizeEntry(entry);
        return fb;
    }
    GfxBindingSetHandle DrawCommandList::createBindingSet(const GfxLogicalBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createBindingSet", "resource");
        if (!m_device || !layout) {
            DO_ERROR("DrawCommandList::createBindingSet: graphics device or layout is unavailable");
            return nullptr;
        }
        auto bs = create_ref<GfxBindingSet>();
        bs->setPendingDesc(desc, layout);
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::BindingSet;
        entry.binding_set = bs;
        registerRealizeEntry(entry);
        return bs;
    }
    GfxGraphicsPipelineHandle DrawCommandList::createGraphicsPipeline(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::createGraphicsPipeline", "resource");
        if (!m_device) {
            DO_ERROR("DrawCommandList::createGraphicsPipeline: graphics device is unavailable");
            return nullptr;
        }
        auto pso = create_ref<GfxGraphicsPipeline>();
        pso->setPendingDesc(desc, info);
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::GraphicsPipeline;
        entry.pipeline = pso;
        registerRealizeEntry(entry);
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
    void DrawCommandList::CopyTextureToStagingCommand::execute(GfxCommandList& c) const { if (m_src->isGpuReady()) c.copyTexture(m_dst.Get(), m_dst_slice, m_src->getRHIHandle().Get(), m_src_slice); }
    void DrawCommandList::ResolveTextureCommand::execute(GfxCommandList& c) const { if (m_dst->isGpuReady() && m_src->isGpuReady()) c.resolveTexture(m_dst->getRHIHandle().Get(), m_subresources, m_src->getRHIHandle().Get(), m_subresources); }
    void DrawCommandList::SetTextureStateCommand::execute(GfxCommandList& c) const { if (m_t->isGpuReady()) c.setTextureState(m_t->getRHIHandle(), m_s, m_st); else DO_WARN("SetTextureState: texture not realized"); }
    void DrawCommandList::SetBufferStateCommand::execute(GfxCommandList& c) const { if (m_b->isGpuReady()) c.setBufferState(m_b->getRHIHandle(), m_st); else DO_WARN("SetBufferState: buffer not realized"); }

    namespace {

        UnorderedSet<const void*>& ReplayWarnKeys() {
            static UnorderedSet<const void*> s_keys;
            return s_keys;
        }

        Bool ShouldWarnReplay(const void* key) {
            return ReplayWarnKeys().insert(key).second;
        }

        void ClearReplayWarn(const void* key) {
            ReplayWarnKeys().erase(key);
        }

        Bool ResolveBindingSet(GfxGraphicsState& state, const GfxBindingSetHandle& binding_set) {
            if (!binding_set) return true;
            if (!binding_set->isGpuReady()) {
                if (ShouldWarnReplay(binding_set.Get())) {
                    DO_WARN("SetGraphicsState: binding set not ready at replay");
                }
                return false;
            }
            ClearReplayWarn(binding_set.Get());
            state.addBindingSet(binding_set->getRHIHandle());
            return true;
        }

        Bool ResolveVertexBinding(GfxGraphicsState& state, const GfxVertexBindingDesc& binding) {
            if (!binding.buffer) return true;
            if (!binding.buffer->isGpuReady()) {
                if (ShouldWarnReplay(binding.buffer.Get())) {
                    DO_WARN("SetGraphicsState: vertex buffer not ready at replay (slot={})", binding.slot);
                }
                return false;
            }
            ClearReplayWarn(binding.buffer.Get());
            state.addVertexBuffer(
                GfxVertexBufferBinding()
                    .setBuffer(binding.buffer->getRHIHandle())
                    .setSlot(binding.slot)
                    .setOffset(binding.offset));
            return true;
        }

    } // namespace

    void DrawCommandList::SetGraphicsStateCommand::execute(GfxCommandList& cm) const {
        const GfxDrawState& state = m_state;
        Bool blocked = false;

        GfxGraphicsState s;
        s.setViewport(state.viewport);
        if (state.pipeline) {
            if (state.pipeline->isGpuReady()) {
                ClearReplayWarn(state.pipeline.Get());
                s.setPipeline(state.pipeline->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.pipeline.Get())) {
                    DO_WARN("SetGraphicsState: pipeline not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.framebuffer) {
            if (state.framebuffer->isGpuReady()) {
                ClearReplayWarn(state.framebuffer.Get());
                s.setFramebuffer(state.framebuffer->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.framebuffer.Get())) {
                    DO_WARN("SetGraphicsState: framebuffer not ready at replay");
                }
                blocked = true;
            }
        }
        for (const auto& binding_set : state.bindings) {
            if (!ResolveBindingSet(s, binding_set)) blocked = true;
        }
        for (const auto& vertex_buffer : state.vertex_buffers) {
            if (!ResolveVertexBinding(s, vertex_buffer)) blocked = true;
        }
        if (state.index_buffer.buffer) {
            if (state.index_buffer.buffer->isGpuReady()) {
                ClearReplayWarn(state.index_buffer.buffer.Get());
                s.setIndexBuffer(
                    GfxIndexBufferBinding()
                        .setBuffer(state.index_buffer.buffer->getRHIHandle())
                        .setFormat(state.index_buffer.format)
                        .setOffset(state.index_buffer.offset));
            } else {
                if (ShouldWarnReplay(state.index_buffer.buffer.Get())) {
                    DO_WARN("SetGraphicsState: index buffer not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_params) {
            if (state.indirect_params->isGpuReady()) {
                ClearReplayWarn(state.indirect_params.Get());
                s.setIndirectParams(state.indirect_params->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_params.Get())) {
                    DO_WARN("SetGraphicsState: indirect params not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_count_buffer) {
            if (state.indirect_count_buffer->isGpuReady()) {
                ClearReplayWarn(state.indirect_count_buffer.Get());
                s.setIndirectCountBuffer(state.indirect_count_buffer->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_count_buffer.Get())) {
                    DO_WARN("SetGraphicsState: indirect count buffer not ready at replay");
                }
                blocked = true;
            }
        }
        if (blocked) return;
        cm.setGraphicsState(s);
    }

    void DrawCommandList::SetComputeStateCommand::execute(GfxCommandList& cm) const {
        const GfxComputeDispatchState& state = m_state;
        Bool blocked = false;

        GfxComputeState s;
        s.setPipeline(state.pipeline);
        for (const auto& binding_set : state.bindings) {
            if (!binding_set) continue;
            if (binding_set->isGpuReady()) {
                ClearReplayWarn(binding_set.Get());
                s.addBindingSet(binding_set->getRHIHandle());
            } else {
                if (ShouldWarnReplay(binding_set.Get())) {
                    DO_WARN("SetComputeState: binding set not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_params) {
            if (state.indirect_params->isGpuReady()) {
                ClearReplayWarn(state.indirect_params.Get());
                s.setIndirectParams(state.indirect_params->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_params.Get())) {
                    DO_WARN("SetComputeState: indirect params not ready at replay");
                }
                blocked = true;
            }
        }
        if (blocked) return;
        cm.setComputeState(s);
    }

    Size_t DrawCommandList::alignUp(Size_t v, Size_t a) {
        DO_ASSERT(a != 0 && (a & (a - 1)) == 0, "alignUp: invalid alignment");
        return (v + a - 1) & ~(a - 1);
    }

} // dodoe
