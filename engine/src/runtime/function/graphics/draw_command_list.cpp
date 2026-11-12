// do@Redlive

#include "draw_command_list.h"
#include "gfx_context.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"
#include <limits>

namespace dodoe {

    DrawCommandList GDrawCommandList;

    DrawCommandList::DrawCommandList(DrawCommandList&& other) noexcept {
        *this = std::move(other);
    }

    DrawCommandList& DrawCommandList::operator=(DrawCommandList&& other) noexcept {
        if (this == &other) return *this;
        std::scoped_lock lock(m_record_mutex, other.m_record_mutex);
        CommandList<DrawCommandReplayContext>::operator=(std::move(other));
        m_device = std::move(other.m_device);
        m_realize_plan = std::move(other.m_realize_plan);
        m_realize_resources = std::move(other.m_realize_resources);
        other.m_realize_plan.clear();
        other.m_realize_resources.clear();
        return *this;
    }

    void DrawCommandList::setDevice(GfxContext& gfx) {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::setDevice", "startup");
        m_device = gfx.getDevice();
        if (!m_device) {
            DO_ERROR("DrawCommandList::setDevice: graphics device is unavailable");
        }
    }

    void DrawCommandList::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("DrawCommandList::shutdown", "shutdown");
        beginFrame();
        m_device = nullptr;
    }

    void DrawCommandList::beginFrame() {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        resetAndRelease();
        m_realize_plan.clear();
        m_realize_resources.clear();
    }
    void DrawCommandList::endFrame() {}

    DrawCommandList DrawCommandList::detachRecordedCommands() {
        DrawCommandList detached{};
        {
            std::lock_guard<std::mutex> lock(m_record_mutex);
            detached.m_device = m_device;
            static_cast<CommandList<DrawCommandReplayContext>&>(detached) = CommandList<DrawCommandReplayContext>(std::move(*this));
            detached.m_realize_plan = std::move(m_realize_plan);
            detached.m_realize_resources = std::move(m_realize_resources);
            m_realize_plan.clear();
            m_realize_resources.clear();
        }
        return detached;
    }

    void DrawCommandList::execute(const GfxCommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        execute(*command_list);
    }

    void DrawCommandList::execute(GfxCommandList& command_list) const {
        DrawCommandReplayContext context{command_list};
        CommandList::execute(context);
    }

    void DrawCommandList::registerRealizeEntry(const GfxResourceRealizeEntry& entry) {
        const void* resource = nullptr;
        switch (entry.kind) {
            case GfxResourceRealizeEntry::Kind::Texture: resource = entry.texture.get(); break;
            case GfxResourceRealizeEntry::Kind::Buffer: resource = entry.buffer.get(); break;
            case GfxResourceRealizeEntry::Kind::Framebuffer: resource = entry.framebuffer.get(); break;
            case GfxResourceRealizeEntry::Kind::BindingSet: resource = entry.binding_set.get(); break;
            case GfxResourceRealizeEntry::Kind::GraphicsPipeline: resource = entry.pipeline.get(); break;
            case GfxResourceRealizeEntry::Kind::DescriptorWrite: resource = entry.descriptor_write.get(); break;
        }
        if (!resource) return;
        std::lock_guard<std::mutex> lock(m_record_mutex);
        if (m_realize_resources.insert(resource).second) m_realize_plan.push_back(entry);
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
        DO_ASSERT(this != &other, "Cannot append a command list to itself");
        if (this == &other) return;
        if (!other.m_realize_plan.empty()) {
            for (auto& entry : other.m_realize_plan) {
                registerRealizeEntry(entry);
            }
            other.m_realize_plan.clear();
            other.m_realize_resources.clear();
        }
        CommandList<DrawCommandReplayContext>::append(std::move(other));
    }

    void DrawCommandList::open()  { recordCommand<OpenCommand>(); }
    void DrawCommandList::close() { recordCommand<CloseCommand>(); }
    void DrawCommandList::clearState() { recordCommand<ClearStateCommand>(); }

    void DrawCommandList::beginMarker(const char* name) {
        BeginMarkerCommand::Create(*this, name);
    }
    void DrawCommandList::endMarker() { recordCommand<EndMarkerCommand>(); }

    void DrawCommandList::clearTextureFloat(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color) {
        requestRealize(texture);
        recordCommand<ClearTextureFloatCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearTextureUInt(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, UInt32 clear_color) {
        requestRealize(texture);
        recordCommand<ClearTextureUIntCommand>(texture, subresources, clear_color);
    }
    void DrawCommandList::clearDepthStencilTexture(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, Bool clear_depth, Float depth, Bool clear_stencil, UInt8 stencil) {
        requestRealize(texture);
        recordCommand<ClearDepthStencilTextureCommand>(texture, subresources, clear_depth, depth, clear_stencil, stencil);
    }
    void DrawCommandList::copyBuffer(const GfxBufferHandle& destination, UInt64 destination_offset_bytes, const GfxBufferHandle& source, UInt64 source_offset_bytes, UInt64 data_size_bytes) {
        requestRealize(destination);
        requestRealize(source);
        recordCommand<CopyBufferCommand>(destination, destination_offset_bytes, source, source_offset_bytes, data_size_bytes);
    }
    void DrawCommandList::copyTextureToStaging(const GfxStagingTextureHandle& destination, const GfxTextureSlice& destination_slice, const GfxTextureHandle& source, const GfxTextureSlice& source_slice) {
        requestRealize(source);
        recordCommand<CopyTextureToStagingCommand>(destination, destination_slice, source, source_slice);
    }

    void DrawCommandList::resolveTexture(const GfxTextureHandle& destination, const GfxTextureHandle& source, const GfxTextureSubresourceSet& subresources) {
        requestRealize(destination);
        requestRealize(source);
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
        const auto capacity = buffer->getDesc().byteSize;
        if (!data || destination_offset_bytes > capacity || data_size > capacity - destination_offset_bytes) {
            DO_ERROR("DrawCommandList::writeBuffer: invalid data or upload range (size={})", data_size);
            return;
        }
        if (!buffer->isGpuReady()) {
            DO_DEBUG("DrawCommandList::writeBuffer: buffer pending realize, upload replayed after prepare");
        }
        requestRealize(buffer);
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
        if (mip_level >= desc.mipLevels || mip_level >= 32 || array_slice >= desc.arraySize) {
            DO_ERROR("DrawCommandList::writeTexture: invalid mip or array slice");
            return;
        }
        const auto& format = cutie::getFormatInfo(desc.format);
        const Size_t block = std::max<Size_t>(format.blockSize, 1);
        const Size_t width = std::max(desc.width >> mip_level, 1u);
        const Size_t height = std::max(desc.height >> mip_level, 1u);
        const Size_t rows = (height + block - 1) / block;
        if (!format.bytesPerBlock || row_pitch < ((width + block - 1) / block) * format.bytesPerBlock ||
            row_pitch > (std::numeric_limits<Size_t>::max)() / rows) {
            DO_ERROR("DrawCommandList::writeTexture: invalid row pitch");
            return;
        }
        const Size_t data_size = rows * row_pitch;
        requestRealize(texture);
        WriteTextureCommand::Create(*this, texture, mip_level, array_slice, data, row_pitch, data_size);
    }

    void DrawCommandList::setPushConstants(const void* data, Size_t byte_size) {
        PushConstantsCommand::Create(*this, data, byte_size);
    }

    void DrawCommandList::setTextureState(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, GfxResourceStates state_bits) {
        requestRealize(texture);
        recordCommand<SetTextureStateCommand>(texture, subresources, state_bits);
    }
    void DrawCommandList::setBufferState(const GfxBufferHandle& buffer, GfxResourceStates state_bits) {
        requestRealize(buffer);
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
        setGraphicsState(state);
    }
    void DrawCommandList::setGraphicsState(const GfxDrawState& state) {
        requestRealize(state.framebuffer);
        requestRealize(state.pipeline);
        for (const auto& binding : state.bindings) requestRealize(binding);
        for (const auto& binding : state.vertex_buffers) requestRealize(binding.buffer);
        requestRealize(state.index_buffer.buffer);
        requestRealize(state.indirect_params);
        requestRealize(state.indirect_count_buffer);
        recordCommand<SetGraphicsStateCommand>(state);
    }
    void DrawCommandList::setComputeState(const GfxComputeDispatchState& state) {
        for (const auto& binding : state.bindings) requestRealize(binding);
        requestRealize(state.indirect_params);
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
            const auto& format = cutie::getFormatInfo(desc.format);
            const Size_t block = std::max<Size_t>(format.blockSize, 1);
            const Size_t pitch = ((static_cast<Size_t>(desc.width) + block - 1) / block) * format.bytesPerBlock;
            const Size_t rows = (static_cast<Size_t>(desc.height) + block - 1) / block;
            if (!pitch || !rows || pitch > data_size / rows) {
                DO_ERROR("DrawCommandList::createTexture: initial data is too small");
                return {};
            }
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
        if (!table || !texture) return -1;
        auto write = create_ref<GfxDescriptorWrite>();
        write->table = table;
        write->texture = texture;
        write->slot = slot;
        GfxResourceRealizeEntry entry{};
        entry.kind = GfxResourceRealizeEntry::Kind::DescriptorWrite;
        entry.descriptor_write = std::move(write);
        registerRealizeEntry(entry);
        return static_cast<DescriptorIndex>(slot);
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

    void DrawCommandList::OpenCommand::execute(DrawCommandReplayContext& c) const { c.graphics_state_valid = c.compute_state_valid = false; c.target.open(); }
    void DrawCommandList::CloseCommand::execute(DrawCommandReplayContext& c) const { c.target.close(); }
    void DrawCommandList::ClearStateCommand::execute(DrawCommandReplayContext& c) const { c.graphics_state_valid = c.compute_state_valid = false; c.target.clearState(); }
    void DrawCommandList::EndMarkerCommand::execute(DrawCommandReplayContext& c) const { c.target.endMarker(); }
    void DrawCommandList::CommitBarriersCommand::execute(DrawCommandReplayContext& c) const { c.target.commitBarriers(); }
    void DrawCommandList::DrawPrimitiveCommand::execute(DrawCommandReplayContext& c) const { if (c.graphics_state_valid) c.target.draw(m_args); }
    void DrawCommandList::DrawIndexedPrimitiveCommand::execute(DrawCommandReplayContext& c) const { if (c.graphics_state_valid) c.target.drawIndexed(m_args); }
    void DrawCommandList::DispatchCommand::execute(DrawCommandReplayContext& c) const { if (c.compute_state_valid) c.target.dispatch(m_x, m_y, m_z); }
    void DrawCommandList::DrawIndirectCommand::execute(DrawCommandReplayContext& c) const { if (c.graphics_state_valid) c.target.drawIndirect(m_off, m_cnt); }
    void DrawCommandList::DrawIndexedIndirectCommand::execute(DrawCommandReplayContext& c) const { if (c.graphics_state_valid) c.target.drawIndexedIndirect(m_off, m_cnt); }
    void DrawCommandList::DispatchIndirectCommand::execute(DrawCommandReplayContext& c) const { if (c.compute_state_valid) c.target.dispatchIndirect(m_off); }

    void DrawCommandList::ClearTextureFloatCommand::execute(DrawCommandReplayContext& c) const { if (m_texture->isGpuReady()) c.target.clearTextureFloat(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearTextureUIntCommand::execute(DrawCommandReplayContext& c) const { if (m_texture->isGpuReady()) c.target.clearTextureUInt(m_texture->getRHIHandle(), m_subresources, m_clear_color); }
    void DrawCommandList::ClearDepthStencilTextureCommand::execute(DrawCommandReplayContext& c) const { if (m_texture->isGpuReady()) c.target.clearDepthStencilTexture(m_texture->getRHIHandle(), m_subresources, m_clear_depth, m_depth, m_clear_stencil, m_stencil); }
    void DrawCommandList::CopyBufferCommand::execute(DrawCommandReplayContext& c) const { if (m_dst->isGpuReady() && m_src->isGpuReady()) c.target.copyBuffer(m_dst->getRHIHandle(), m_dst_off, m_src->getRHIHandle(), m_src_off, m_size); }
    void DrawCommandList::CopyTextureToStagingCommand::execute(DrawCommandReplayContext& c) const { if (m_src->isGpuReady()) c.target.copyTexture(m_dst.Get(), m_dst_slice, m_src->getRHIHandle().Get(), m_src_slice); }
    void DrawCommandList::ResolveTextureCommand::execute(DrawCommandReplayContext& c) const { if (m_dst->isGpuReady() && m_src->isGpuReady()) c.target.resolveTexture(m_dst->getRHIHandle().Get(), m_subresources, m_src->getRHIHandle().Get(), m_subresources); }
    void DrawCommandList::SetTextureStateCommand::execute(DrawCommandReplayContext& c) const { if (m_t->isGpuReady()) c.target.setTextureState(m_t->getRHIHandle(), m_s, m_st); else DO_WARN("SetTextureState: texture not realized"); }
    void DrawCommandList::SetBufferStateCommand::execute(DrawCommandReplayContext& c) const { if (m_b->isGpuReady()) c.target.setBufferState(m_b->getRHIHandle(), m_st); else DO_WARN("SetBufferState: buffer not realized"); }

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
            if (!binding_set) return false;
            if (!binding_set->isGpuReady()) {
                if (ShouldWarnReplay(binding_set.get())) {
                    DO_WARN("SetGraphicsState: binding set not ready at replay");
                }
                return false;
            }
            ClearReplayWarn(binding_set.get());
            state.addBindingSet(binding_set->getRHIHandle());
            return true;
        }

        Bool ResolveVertexBinding(GfxGraphicsState& state, const GfxVertexBindingDesc& binding) {
            if (!binding.buffer) return false;
            if (!binding.buffer->isGpuReady()) {
                if (ShouldWarnReplay(binding.buffer.get())) {
                    DO_WARN("SetGraphicsState: vertex buffer not ready at replay (slot={})", binding.slot);
                }
                return false;
            }
            ClearReplayWarn(binding.buffer.get());
            state.addVertexBuffer(
                GfxVertexBufferBinding()
                    .setBuffer(binding.buffer->getRHIHandle())
                    .setSlot(binding.slot)
                    .setOffset(binding.offset));
            return true;
        }

    } // namespace

    void DrawCommandList::SetGraphicsStateCommand::execute(DrawCommandReplayContext& cm) const {
        cm.graphics_state_valid = cm.compute_state_valid = false;
        const GfxDrawState& state = m_state;
        Bool blocked = !state.pipeline || !state.framebuffer;

        GfxGraphicsState s;
        s.setViewport(state.viewport);
        s.setBlendColor(state.blend_constant_color);
        s.setDynamicStencilRefValue(state.dynamic_stencil_ref_value);
        if (state.pipeline) {
            if (state.pipeline->isGpuReady()) {
                ClearReplayWarn(state.pipeline.get());
                s.setPipeline(state.pipeline->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.pipeline.get())) {
                    DO_WARN("SetGraphicsState: pipeline not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.framebuffer) {
            if (state.framebuffer->isGpuReady()) {
                ClearReplayWarn(state.framebuffer.get());
                s.setFramebuffer(state.framebuffer->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.framebuffer.get())) {
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
                ClearReplayWarn(state.index_buffer.buffer.get());
                s.setIndexBuffer(
                    GfxIndexBufferBinding()
                        .setBuffer(state.index_buffer.buffer->getRHIHandle())
                        .setFormat(state.index_buffer.format)
                        .setOffset(state.index_buffer.offset));
            } else {
                if (ShouldWarnReplay(state.index_buffer.buffer.get())) {
                    DO_WARN("SetGraphicsState: index buffer not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_params) {
            if (state.indirect_params->isGpuReady()) {
                ClearReplayWarn(state.indirect_params.get());
                s.setIndirectParams(state.indirect_params->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_params.get())) {
                    DO_WARN("SetGraphicsState: indirect params not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_count_buffer) {
            if (state.indirect_count_buffer->isGpuReady()) {
                ClearReplayWarn(state.indirect_count_buffer.get());
                s.setIndirectCountBuffer(state.indirect_count_buffer->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_count_buffer.get())) {
                    DO_WARN("SetGraphicsState: indirect count buffer not ready at replay");
                }
                blocked = true;
            }
        }
        if (blocked) return;
        cm.target.setGraphicsState(s);
        cm.graphics_state_valid = true;
    }

    void DrawCommandList::SetComputeStateCommand::execute(DrawCommandReplayContext& cm) const {
        cm.graphics_state_valid = cm.compute_state_valid = false;
        const GfxComputeDispatchState& state = m_state;
        Bool blocked = !state.pipeline;

        GfxComputeState s;
        s.setPipeline(state.pipeline);
        for (const auto& binding_set : state.bindings) {
            if (!binding_set) { blocked = true; continue; }
            if (binding_set->isGpuReady()) {
                ClearReplayWarn(binding_set.get());
                s.addBindingSet(binding_set->getRHIHandle());
            } else {
                if (ShouldWarnReplay(binding_set.get())) {
                    DO_WARN("SetComputeState: binding set not ready at replay");
                }
                blocked = true;
            }
        }
        if (state.indirect_params) {
            if (state.indirect_params->isGpuReady()) {
                ClearReplayWarn(state.indirect_params.get());
                s.setIndirectParams(state.indirect_params->getRHIHandle());
            } else {
                if (ShouldWarnReplay(state.indirect_params.get())) {
                    DO_WARN("SetComputeState: indirect params not ready at replay");
                }
                blocked = true;
            }
        }
        if (blocked) return;
        cm.target.setComputeState(s);
        cm.compute_state_valid = true;
    }

    Size_t DrawCommandList::alignUp(Size_t v, Size_t a) {
        DO_ASSERT(a != 0 && (a & (a - 1)) == 0, "alignUp: invalid alignment");
        return (v + a - 1) & ~(a - 1);
    }

} // dodoe
