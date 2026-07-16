#include "draw_command_list.h"
#include "gfx_context.h"

namespace dodoe {

    DrawCommandList GDrawCommandList;

    void DrawCommandList::setDevice(GfxContext& gfx) {
        m_device = gfx.getDevice();
    }

    ImmediateFrameScope::ImmediateFrameScope(GfxDeviceHandle device, GfxContext* gfx, UInt32 image_index)
        : m_device(device), m_gfx(gfx), m_image_index(image_index) {
        m_cmd = device->createCommandList();
        m_cmd->open();
        GDrawCommandList.beginImmediateFrame(*m_cmd);
    }

    ImmediateFrameScope::~ImmediateFrameScope() {
        if (GDrawCommandList.isImmediate()) {
            GDrawCommandList.endImmediateFrame();
        }
        m_cmd->close();
        m_device->executeCommandList(m_cmd);

        m_gfx->presentSwapchainImage(m_image_index);
        m_gfx->clearGarbage();
        GDrawCommandList.reset();
    }

    void ImmediateFrameScope::flush() {
    }

    DrawCommandList::DrawCommand::DrawCommand(Size_t size, ExecuteFunction execute, DestroyFunction destroy)
        : m_size(size), m_execute(execute), m_destroy(destroy) {}

    DrawCommandList::DrawCommandList() : DrawCommandList(kDefaultBlockSize) {}

    DrawCommandList::DrawCommandList(Size_t default_block_size) {
        (void)default_block_size;
    }

    DrawCommandList::~DrawCommandList() {
        reset();
    }

    DrawCommandList::DrawCommandList(DrawCommandList&& other) noexcept {
        moveFrom(std::move(other));
    }

    DrawCommandList& DrawCommandList::operator=(DrawCommandList&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();
        moveFrom(std::move(other));
        return *this;
    }

    void DrawCommandList::beginFrame() {
        reset();
    }

    void DrawCommandList::endFrame() {
    }

    void DrawCommandList::reset() {
        DrawCommandList::DrawCommand* command = m_head;
        while (command != nullptr) {
            DrawCommandList::DrawCommand* next = command->m_next;
            DO_ASSERT(command->m_destroy != nullptr, "DrawCommandList destroy function is null");
            command->m_destroy(*command);
            command = next;
        }

        m_head = nullptr;
        m_tail = nullptr;
        m_command_count = 0;
        Memory::AdvanceFrameEpoch();
    }

    void DrawCommandList::append(DrawCommandList&& other) {
        if (other.isEmpty()) {
            return;
        }

        if (m_tail != nullptr) {
            m_tail->m_next = other.m_head;
        }
        else {
            m_head = other.m_head;
        }

        m_tail = other.m_tail;
        m_command_count += other.m_command_count;
    }

    void DrawCommandList::execute(GfxCommandList& command_list) const {
        const DrawCommandList::DrawCommand* command = m_head;
        while (command != nullptr) {
            DO_ASSERT(command->m_execute != nullptr, "DrawCommandList execute function is null");
            command->m_execute(*command, command_list);
            command = command->m_next;
        }
    }

    void DrawCommandList::execute(const GfxCommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        execute(*command_list);
    }

    Bool DrawCommandList::isEmpty() const {
        return m_head == nullptr;
    }

    Size_t DrawCommandList::commandCount() const {
        return m_command_count;
    }

    void DrawCommandList::open() {
        if (m_immediate_target) {
            m_immediate_target->open();
            return;
        }
        enqueue<OpenCommand>();
    }

    void DrawCommandList::close() {
        if (m_immediate_target) {
            m_immediate_target->close();
            return;
        }
        enqueue<CloseCommand>();
    }

    void DrawCommandList::clearState() {
        if (m_immediate_target) {
            m_immediate_target->clearState();
            return;
        }
        enqueue<ClearStateCommand>();
    }

    void DrawCommandList::beginMarker(const char* name) {
        if (m_immediate_target) {
            m_immediate_target->beginMarker(name);
            return;
        }
        BeginMarkerCommand::Create(*this, name);
    }

    void DrawCommandList::endMarker() {
        if (m_immediate_target) {
            m_immediate_target->endMarker();
            return;
        }
        enqueue<EndMarkerCommand>();
    }

    void DrawCommandList::clearTextureFloat(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        const GfxColor& clear_color)
    {
        if (m_immediate_target) {
            m_immediate_target->clearTextureFloat(texture->getRHIHandle(), subresources, clear_color);
            return;
        }
        enqueue<ClearTextureFloatCommand>(texture, subresources, clear_color);
    }

    void DrawCommandList::clearTextureUInt(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        UInt32 clear_color)
    {
        if (m_immediate_target) {
            m_immediate_target->clearTextureUInt(texture->getRHIHandle(), subresources, clear_color);
            return;
        }
        enqueue<ClearTextureUIntCommand>(texture, subresources, clear_color);
    }

    void DrawCommandList::clearDepthStencilTexture(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        Bool clear_depth,
        Float depth,
        Bool clear_stencil,
        UInt8 stencil)
    {
        if (m_immediate_target) {
            m_immediate_target->clearDepthStencilTexture(texture->getRHIHandle(), subresources, clear_depth, depth, clear_stencil, stencil);
            return;
        }
        enqueue<ClearDepthStencilTextureCommand>(texture, subresources, clear_depth, depth, clear_stencil, stencil);
    }

    void DrawCommandList::copyBuffer(
        const GfxBufferHandle& destination,
        UInt64 destination_offset_bytes,
        const GfxBufferHandle& source,
        UInt64 source_offset_bytes,
        UInt64 data_size_bytes)
    {
        if (m_immediate_target) {
            m_immediate_target->copyBuffer(destination->getRHIHandle(), destination_offset_bytes, source->getRHIHandle(), source_offset_bytes, data_size_bytes);
            return;
        }
        enqueue<CopyBufferCommand>(destination, destination_offset_bytes, source, source_offset_bytes, data_size_bytes);
    }

    void DrawCommandList::writeBuffer(
        const GfxBufferHandle& buffer,
        const void* data,
        Size_t data_size,
        UInt64 destination_offset_bytes)
    {
        if (m_immediate_target) {
            m_immediate_target->writeBuffer(buffer->getRHIHandle(), data, data_size, destination_offset_bytes);
            return;
        }
        WriteBufferCommand::Create(*this, buffer, data, data_size, destination_offset_bytes);
    }

    void DrawCommandList::writeTexture(
        const GfxTextureHandle& texture,
        UInt32 mip_level,
        UInt32 array_slice,
        const void* data,
        Size_t row_pitch)
    {
        DO_ASSERT(texture != nullptr, "DrawCommandList writeTexture: texture is null");
        if (m_immediate_target) {
            m_immediate_target->writeTexture(texture->getRHIHandle(), mip_level, array_slice, data, row_pitch);
            return;
        }
        const UInt32 bytes_per_pixel = texture->getFormat() == GfxFormat::RGBA32_FLOAT ? 16u : 4u;
        const Size_t data_size = static_cast<Size_t>(texture->getHeight()) * row_pitch;
        WriteTextureCommand::Create(*this, texture, mip_level, array_slice, data, row_pitch, data_size);
    }

    void DrawCommandList::setPushConstants(const void* data, Size_t byte_size) {
        if (m_immediate_target) {
            m_immediate_target->setPushConstants(data, byte_size);
            return;
        }
        PushConstantsCommand::Create(*this, data, byte_size);
    }

    void DrawCommandList::setTextureState(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        GfxResourceStates state_bits)
    {
        if (m_immediate_target) {
            m_immediate_target->setTextureState(texture->getRHIHandle(), subresources, state_bits);
            return;
        }
        enqueue<SetTextureStateCommand>(texture, subresources, state_bits);
    }

    void DrawCommandList::setBufferState(const GfxBufferHandle& buffer, const GfxResourceStates state_bits) {
        if (m_immediate_target) {
            m_immediate_target->setBufferState(buffer->getRHIHandle(), state_bits);
            return;
        }
        enqueue<SetBufferStateCommand>(buffer, state_bits);
    }

    void DrawCommandList::commitBarriers() {
        if (m_immediate_target) {
            m_immediate_target->commitBarriers();
            return;
        }
        enqueue<CommitBarriersCommand>();
    }

    void DrawCommandList::setGraphicsState(
        const GfxFramebufferHandle& framebuffer,
        const GfxGraphicsPipelineHandle& pipeline,
        const DynamicArray<GfxBindingSetHandle>& binding_sets,
        const GfxViewportState& viewport,
        const DynamicArray<GfxVertexBufferBinding>& vertex_buffers,
        const GfxIndexBufferBinding& index_buffer)
    {
        if (m_immediate_target) {
            GfxGraphicsState state;
            state.setViewport(viewport);
            if (pipeline && pipeline->isRHIReady()) state.setPipeline(pipeline->getRHIHandle());
            if (framebuffer && framebuffer->isRHIReady()) state.setFramebuffer(framebuffer->getRHIHandle());
            for (auto& bs : binding_sets) {
                if (bs && bs->isRHIReady()) state.addBindingSet(bs->getRHIHandle());
            }
            for (auto& vb : vertex_buffers) state.addVertexBuffer(vb);
            state.setIndexBuffer(index_buffer);
            m_immediate_target->setGraphicsState(state);
            return;
        }
        enqueue<SetGraphicsStateCommand>(framebuffer, pipeline, binding_sets, viewport, vertex_buffers, index_buffer);
    }

    void DrawCommandList::setGraphicsState(const GfxGraphicsState& state) {
        if (m_immediate_target) {
            m_immediate_target->setGraphicsState(state);
            return;
        }
        enqueue<SetGraphicsStateByValueCommand>(state);
    }

    void DrawCommandList::setComputeState(const GfxComputeState& state) {
        if (m_immediate_target) {
            m_immediate_target->setComputeState(state);
            return;
        }
        enqueue<SetComputeStateCommand>(state);
    }

    void DrawCommandList::draw(const GfxDrawArguments& args) {
        if (m_immediate_target) {
            m_immediate_target->draw(args);
            return;
        }
        enqueue<DrawPrimitiveCommand>(args);
    }

    void DrawCommandList::drawIndexed(const GfxDrawArguments& args) {
        if (m_immediate_target) {
            m_immediate_target->drawIndexed(args);
            return;
        }
        enqueue<DrawIndexedPrimitiveCommand>(args);
    }

    void DrawCommandList::drawIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        if (m_immediate_target) {
            m_immediate_target->drawIndirect(offset_bytes, draw_count);
            return;
        }
        enqueue<DrawIndirectCommand>(offset_bytes, draw_count);
    }

    void DrawCommandList::drawIndexedIndirect(UInt32 offset_bytes, UInt32 draw_count) {
        if (m_immediate_target) {
            m_immediate_target->drawIndexedIndirect(offset_bytes, draw_count);
            return;
        }
        enqueue<DrawIndexedIndirectCommand>(offset_bytes, draw_count);
    }

    void DrawCommandList::dispatch(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z) {
        if (m_immediate_target) {
            m_immediate_target->dispatch(groups_x, groups_y, groups_z);
            return;
        }
        enqueue<DispatchCommand>(groups_x, groups_y, groups_z);
    }

    void DrawCommandList::dispatchIndirect(UInt32 offset_bytes) {
        if (m_immediate_target) {
            m_immediate_target->dispatchIndirect(offset_bytes);
            return;
        }
        enqueue<DispatchIndirectCommand>(offset_bytes);
    }

    GfxTextureHandle DrawCommandList::createTexture(const GfxTextureDesc& desc, const void* data, Size_t data_size) {
        auto texture = create_ref<GfxTexture>(desc);
        texture->initializeRHI(m_device);
        if (data && data_size > 0) {
            const UInt32 bytes_per_pixel = desc.format == GfxFormat::RGBA32_FLOAT ? 16u : 4u;
            const Size_t row_pitch = static_cast<Size_t>(desc.width) * bytes_per_pixel;
            writeTexture(texture, 0, 0, data, row_pitch);
        }
        return texture;
    }

    GfxBufferHandle DrawCommandList::createBuffer(const GfxBufferDesc& desc, const void* data, Size_t data_size) {
        auto buffer = create_ref<GfxBuffer>(desc);
        buffer->initializeRHI(m_device);
        if (data && data_size > 0) {
            writeBuffer(buffer, data, data_size, 0);
        }
        return buffer;
    }

    GfxFramebufferHandle DrawCommandList::createFramebuffer(const GfxFramebufferDesc& desc) {
        auto framebuffer = create_ref<GfxFramebuffer>(desc);
        framebuffer->initializeRHI(m_device);
        return framebuffer;
    }

    GfxBindingSetHandle DrawCommandList::createBindingSet(const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout) {
        auto binding_set = create_ref<GfxBindingSet>();
        binding_set->initializeRHI(m_device, desc, layout);
        return binding_set;
    }

    GfxGraphicsPipelineHandle DrawCommandList::createGraphicsPipeline(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& framebuffer_info) {
        auto pipeline = create_ref<GfxGraphicsPipeline>();
        pipeline->initializeRHI(m_device, desc, framebuffer_info);
        return pipeline;
    }

    DescriptorIndex DrawCommandList::createDescriptor(GfxDescriptorTableHandle descriptor_table, GfxTextureHandle texture, UInt32 slot) {
        if (m_immediate_target) {
            if (texture && texture->isRHIReady()) {
                GfxBindingSetItem item = GfxBindingSetItem::Texture_SRV(0, texture->getRHIHandle());
                item.slot = slot;
                m_device->writeDescriptorTable(descriptor_table, item);
            }
            return 0;
        }
        enqueue<CreateDescriptorCommand>(m_device, descriptor_table, texture, slot);
        return -1;
    }

    GfxSamplerHandle DrawCommandList::createSampler(const GfxSamplerDesc& desc) {
        return m_device->createSampler(desc);
    }

    GfxBindingLayoutHandle DrawCommandList::createBindingLayout(const GfxBindingLayoutDesc& desc) {
        return m_device->createBindingLayout(desc);
    }

    GfxInputLayoutHandle DrawCommandList::createInputLayout(const GfxVertexAttributeDesc* attributes, UInt32 count, GfxShaderHandle shader) {
        return m_device->createInputLayout(attributes, count, shader);
    }

    GfxShaderHandle DrawCommandList::createShader(const GfxShaderDesc& desc, const void* data, Size_t data_size) {
        if (isImmediate()) {
            return m_device->createShader(desc, data, data_size);
        }
        auto shader = m_device->createShader(desc, data, data_size);
        CreateShaderCommand::Create(*this, m_device, desc, shader, data, data_size);
        return shader;
    }

    DrawCommandList::SetGraphicsStateCommand::SetGraphicsStateCommand(
        const GfxFramebufferHandle& framebuffer,
        const GfxGraphicsPipelineHandle& pipeline,
        const DynamicArray<GfxBindingSetHandle>& binding_sets,
        const GfxViewportState& viewport,
        const DynamicArray<GfxVertexBufferBinding>& vertex_buffers,
        const GfxIndexBufferBinding& index_buffer)
        : m_framebuffer(framebuffer)
        , m_pipeline(pipeline)
        , m_binding_sets(binding_sets)
        , m_viewport(viewport)
        , m_vertex_buffers(vertex_buffers)
        , m_index_buffer(index_buffer) {}

    void DrawCommandList::SetGraphicsStateCommand::execute(GfxCommandList& command_list) const {
        GfxGraphicsState state;
        state.setViewport(m_viewport);
        if (m_pipeline && m_pipeline->isRHIReady()) state.setPipeline(m_pipeline->getRHIHandle());
        if (m_framebuffer && m_framebuffer->isRHIReady()) state.setFramebuffer(m_framebuffer->getRHIHandle());
        for (auto& bs : m_binding_sets) {
            if (bs && bs->isRHIReady()) state.addBindingSet(bs->getRHIHandle());
        }
        for (auto& vb : m_vertex_buffers) {
            state.addVertexBuffer(vb);
        }
        state.setIndexBuffer(m_index_buffer);
        command_list.setGraphicsState(state);
    }

    DrawCommandList::SetComputeStateCommand::SetComputeStateCommand(const GfxComputeState& state)
        : m_state(state) {}

    void DrawCommandList::SetComputeStateCommand::execute(GfxCommandList& command_list) const {
        command_list.setComputeState(m_state);
    }

    DrawCommandList::DrawPrimitiveCommand::DrawPrimitiveCommand(const GfxDrawArguments& args)
        : m_args(args) {}

    void DrawCommandList::DrawPrimitiveCommand::execute(GfxCommandList& command_list) const {
        command_list.draw(m_args);
    }

    DrawCommandList::DrawIndexedPrimitiveCommand::DrawIndexedPrimitiveCommand(const GfxDrawArguments& args)
        : m_args(args) {}

    void DrawCommandList::DrawIndexedPrimitiveCommand::execute(GfxCommandList& command_list) const {
        command_list.drawIndexed(m_args);
    }

    DrawCommandList::DispatchCommand::DispatchCommand(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z)
        : m_groups_x(groups_x), m_groups_y(groups_y), m_groups_z(groups_z) {}

    void DrawCommandList::DispatchCommand::execute(GfxCommandList& command_list) const {
        command_list.dispatch(m_groups_x, m_groups_y, m_groups_z);
    }

    DrawCommandList::DrawIndirectCommand::DrawIndirectCommand(UInt32 offset_bytes, UInt32 draw_count)
        : m_offset_bytes(offset_bytes), m_draw_count(draw_count) {}

    void DrawCommandList::DrawIndirectCommand::execute(GfxCommandList& command_list) const {
        command_list.drawIndirect(m_offset_bytes, m_draw_count);
    }

    DrawCommandList::DrawIndexedIndirectCommand::DrawIndexedIndirectCommand(UInt32 offset_bytes, UInt32 draw_count)
        : m_offset_bytes(offset_bytes), m_draw_count(draw_count) {}

    void DrawCommandList::DrawIndexedIndirectCommand::execute(GfxCommandList& command_list) const {
        command_list.drawIndexedIndirect(m_offset_bytes, m_draw_count);
    }

    DrawCommandList::DispatchIndirectCommand::DispatchIndirectCommand(UInt32 offset_bytes)
        : m_offset_bytes(offset_bytes) {}

    void DrawCommandList::DispatchIndirectCommand::execute(GfxCommandList& command_list) const {
        command_list.dispatchIndirect(m_offset_bytes);
    }

    void DrawCommandList::OpenCommand::execute(GfxCommandList& command_list) const {
        command_list.open();
    }

    void DrawCommandList::CloseCommand::execute(GfxCommandList& command_list) const {
        command_list.close();
    }

    void DrawCommandList::ClearStateCommand::execute(GfxCommandList& command_list) const {
        command_list.clearState();
    }

    DrawCommandList::CreateTextureCommand::CreateTextureCommand(
        GfxDeviceHandle device,
        const GfxTextureDesc& desc,
        GfxTextureHandle texture,
        const void* data,
        Size_t data_size)
        : m_device(device), m_desc(desc), m_texture(std::move(texture)), m_data_size(data_size) {}

    void DrawCommandList::CreateTextureCommand::execute(GfxCommandList& command_list) const {
        m_texture->initializeRHI(m_device);
        if (m_data_size > 0) {
            const UInt32 bytes_per_pixel = m_desc.format == GfxFormat::RGBA32_FLOAT ? 16 : 4;
            const Size_t row_pitch = static_cast<Size_t>(m_desc.width) * bytes_per_pixel;
            command_list.writeTexture(m_texture->getRHIHandle(), 0, 0, this + 1, row_pitch);
        }
    }

    DrawCommandList::CreateBufferCommand::CreateBufferCommand(
        const GfxDeviceHandle device,
        const GfxBufferDesc& desc,
        GfxBufferHandle buffer,
        const Size_t data_size)
        : m_device(device), m_desc(desc), m_buffer(std::move(buffer)), m_data_size(data_size) {}

    DrawCommandList::CreateBufferCommand& DrawCommandList::CreateBufferCommand::Create(
        DrawCommandList& command_list,
        const GfxDeviceHandle device,
        const GfxBufferDesc& desc,
        GfxBufferHandle buffer,
        const void* const data,
        const Size_t data_size)
    {
        void* memory = command_list.allocate(CalculateSize(data_size), alignof(CreateBufferCommand));
        auto* command = new (memory) CreateBufferCommand(device, desc, std::move(buffer), data_size);
        if (data && data_size > 0) {
            std::memcpy(command->mutableData(), data, data_size);
        }
        command_list.appendCommand(command);
        return *command;
    }

    Size_t DrawCommandList::CreateBufferCommand::CalculateSize(const Size_t data_size) {
        return alignUp(sizeof(CreateBufferCommand) + data_size, alignof(CreateBufferCommand));
    }

    void* DrawCommandList::CreateBufferCommand::mutableData() {
        return reinterpret_cast<UInt8*>(this) + sizeof(CreateBufferCommand);
    }

    const void* DrawCommandList::CreateBufferCommand::data() const {
        return m_data_size > 0 ? reinterpret_cast<const UInt8*>(this) + sizeof(CreateBufferCommand) : nullptr;
    }

    void DrawCommandList::CreateBufferCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const CreateBufferCommand&>(command).execute(command_list);
    }

    void DrawCommandList::CreateBufferCommand::DestroyCommand(DrawCommand& command) {
        static_cast<CreateBufferCommand&>(command).~CreateBufferCommand();
    }

    void DrawCommandList::CreateBufferCommand::execute(GfxCommandList& command_list) const {
        m_buffer->initializeRHI(m_device);
        if (m_data_size > 0) {
            command_list.writeBuffer(m_buffer->getRHIHandle(), data(), m_data_size);
        }
    }

    DrawCommandList::CreateFramebufferCommand::CreateFramebufferCommand(
        GfxDeviceHandle device,
        const GfxFramebufferDesc& desc,
        GfxFramebufferHandle framebuffer)
        : m_device(device), m_desc(desc), m_framebuffer(std::move(framebuffer)) {}

    void DrawCommandList::CreateFramebufferCommand::execute(GfxCommandList& command_list) const {
        (void)command_list;
        m_framebuffer->initializeRHI(m_device);
    }

    DrawCommandList::CreateBindingSetCommand::CreateBindingSetCommand(
        GfxDeviceHandle device,
        const GfxBindingSetDesc& desc,
        const GfxBindingLayoutHandle& layout,
        GfxBindingSetHandle binding_set)
        : m_device(device), m_desc(desc), m_layout(layout), m_binding_set(std::move(binding_set)) {}

    void DrawCommandList::CreateBindingSetCommand::execute(GfxCommandList& command_list) const {
        (void)command_list;
        m_binding_set->initializeRHI(m_device, m_desc, m_layout);
    }

    DrawCommandList::CreateGraphicsPipelineCommand::CreateGraphicsPipelineCommand(
        GfxDeviceHandle device,
        const GfxGraphicsPipelineDesc& desc,
        const GfxFramebufferInfo& framebuffer_info,
        GfxGraphicsPipelineHandle pipeline)
        : m_device(device), m_desc(desc), m_framebuffer_info(framebuffer_info), m_pipeline(std::move(pipeline)) {}

    void DrawCommandList::CreateGraphicsPipelineCommand::execute(GfxCommandList& command_list) const {
        (void)command_list;
        m_pipeline->initializeRHI(m_device, m_desc, m_framebuffer_info);
    }

    DrawCommandList::CreateDescriptorCommand::CreateDescriptorCommand(
        GfxDeviceHandle device,
        GfxDescriptorTableHandle descriptor_table,
        GfxTextureHandle texture,
        UInt32 slot)
        : m_device(device), m_descriptor_table(descriptor_table), m_texture(std::move(texture)), m_slot(slot) {}

    void DrawCommandList::CreateDescriptorCommand::execute(GfxCommandList& command_list) const {
        (void)command_list;
        if (m_texture && m_texture->isRHIReady()) {
            GfxBindingSetItem item = GfxBindingSetItem::Texture_SRV(0, m_texture->getRHIHandle());
            item.slot = m_slot;
            m_device->writeDescriptorTable(m_descriptor_table, item);
        }
    }

    DrawCommandList::CreateShaderCommand::CreateShaderCommand(
        const GfxDeviceHandle device,
        const GfxShaderDesc& desc,
        GfxShaderHandle shader,
        const Size_t data_size)
        : DrawCommand(CalculateSize(data_size), &ExecuteCommand, &DestroyCommand),
          m_device(device), m_desc(desc), m_shader(std::move(shader)), m_data_size(data_size) {}

    DrawCommandList::CreateShaderCommand& DrawCommandList::CreateShaderCommand::Create(
        DrawCommandList& command_list,
        const GfxDeviceHandle device,
        const GfxShaderDesc& desc,
        GfxShaderHandle shader,
        const void* const data,
        const Size_t data_size)
    {
        void* memory = command_list.allocate(CalculateSize(data_size), alignof(CreateShaderCommand));
        auto* command = new (memory) CreateShaderCommand(device, desc, std::move(shader), data_size);
        if (data && data_size > 0) {
            std::memcpy(command->mutableData(), data, data_size);
        }
        command_list.appendCommand(command);
        return *command;
    }

    Size_t DrawCommandList::CreateShaderCommand::CalculateSize(const Size_t data_size) {
        return alignUp(sizeof(CreateShaderCommand) + data_size, alignof(CreateShaderCommand));
    }

    void* DrawCommandList::CreateShaderCommand::mutableData() {
        return reinterpret_cast<UInt8*>(this) + sizeof(CreateShaderCommand);
    }

    const void* DrawCommandList::CreateShaderCommand::data() const {
        return m_data_size > 0 ? reinterpret_cast<const UInt8*>(this) + sizeof(CreateShaderCommand) : nullptr;
    }

    void DrawCommandList::CreateShaderCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const CreateShaderCommand&>(command).execute(command_list);
    }

    void DrawCommandList::CreateShaderCommand::DestroyCommand(DrawCommand& command) {
        static_cast<CreateShaderCommand&>(command).~CreateShaderCommand();
    }

    void DrawCommandList::CreateShaderCommand::execute(GfxCommandList& command_list) const {
        (void)command_list;
        m_device->createShader(m_desc, data(), m_data_size);
    }

    DrawCommandList::ClearTextureFloatCommand::ClearTextureFloatCommand(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        const GfxColor& clear_color)
        : m_texture(texture), m_subresources(subresources), m_clear_color(clear_color) {}

    void DrawCommandList::ClearTextureFloatCommand::execute(GfxCommandList& command_list) const {
        command_list.clearTextureFloat(m_texture->getRHIHandle(), m_subresources, m_clear_color);
    }

    DrawCommandList::ClearTextureUIntCommand::ClearTextureUIntCommand(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        UInt32 clear_color)
        : m_texture(texture), m_subresources(subresources), m_clear_color(clear_color) {}

    void DrawCommandList::ClearTextureUIntCommand::execute(GfxCommandList& command_list) const {
        command_list.clearTextureUInt(m_texture->getRHIHandle(), m_subresources, m_clear_color);
    }

    DrawCommandList::ClearDepthStencilTextureCommand::ClearDepthStencilTextureCommand(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        Bool clear_depth,
        Float depth,
        Bool clear_stencil,
        UInt8 stencil)
        : m_texture(texture),
          m_subresources(subresources),
          m_depth(depth),
          m_clear_depth(clear_depth),
          m_clear_stencil(clear_stencil),
          m_stencil(stencil) {}

    void DrawCommandList::ClearDepthStencilTextureCommand::execute(GfxCommandList& command_list) const {
        command_list.clearDepthStencilTexture(m_texture->getRHIHandle(), m_subresources, m_clear_depth, m_depth, m_clear_stencil, m_stencil);
    }

    DrawCommandList::CopyBufferCommand::CopyBufferCommand(
        const GfxBufferHandle& destination,
        UInt64 destination_offset_bytes,
        const GfxBufferHandle& source,
        UInt64 source_offset_bytes,
        UInt64 data_size_bytes)
        : m_destination(destination),
          m_source(source),
          m_destination_offset_bytes(destination_offset_bytes),
          m_source_offset_bytes(source_offset_bytes),
          m_data_size_bytes(data_size_bytes) {}

    void DrawCommandList::CopyBufferCommand::execute(GfxCommandList& command_list) const {
        command_list.copyBuffer(m_destination->getRHIHandle(), m_destination_offset_bytes, m_source->getRHIHandle(), m_source_offset_bytes, m_data_size_bytes);
    }

    DrawCommandList::WriteBufferCommand& DrawCommandList::WriteBufferCommand::Create(
        DrawCommandList& command_list,
        const GfxBufferHandle& buffer,
        const void* data,
        Size_t data_size,
        UInt64 destination_offset_bytes)
    {
        DO_ASSERT(data != nullptr || data_size == 0, "DrawCommandList write buffer data is null");

        void* memory = command_list.allocate(CalculateSize(data_size), alignof(WriteBufferCommand));
        auto* command = new (memory) WriteBufferCommand(buffer, destination_offset_bytes, data_size);
        if (data_size > 0) {
            std::memcpy(command->mutableData(), data, data_size);
        }

        command_list.appendCommand(command);
        return *command;
    }

    const void* DrawCommandList::WriteBufferCommand::data() const {
        return reinterpret_cast<const UInt8*>(this) + sizeof(WriteBufferCommand);
    }

    DrawCommandList::WriteBufferCommand::WriteBufferCommand(
        const GfxBufferHandle& buffer,
        UInt64 destination_offset_bytes,
        Size_t data_size)
        : DrawCommand(CalculateSize(data_size), &WriteBufferCommand::ExecuteCommand, &WriteBufferCommand::DestroyCommand),
          m_buffer(buffer),
          m_destination_offset_bytes(destination_offset_bytes),
          m_data_size(data_size) {}

    void DrawCommandList::WriteBufferCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const WriteBufferCommand&>(command).execute(command_list);
    }

    void DrawCommandList::WriteBufferCommand::DestroyCommand(DrawCommand& command) {
        static_cast<WriteBufferCommand&>(command).~WriteBufferCommand();
    }

    Size_t DrawCommandList::WriteBufferCommand::CalculateSize(Size_t data_size) {
        return sizeof(WriteBufferCommand) + data_size;
    }

    void* DrawCommandList::WriteBufferCommand::mutableData() {
        return reinterpret_cast<UInt8*>(this) + sizeof(WriteBufferCommand);
    }

    void DrawCommandList::WriteBufferCommand::execute(GfxCommandList& command_list) const {
        command_list.writeBuffer(m_buffer->getRHIHandle(), data(), m_data_size, m_destination_offset_bytes);
    }

    DrawCommandList::WriteTextureCommand& DrawCommandList::WriteTextureCommand::Create(
        DrawCommandList& command_list,
        const GfxTextureHandle& texture,
        UInt32 mip_level,
        UInt32 array_slice,
        const void* data,
        Size_t row_pitch,
        Size_t data_size)
    {
        DO_ASSERT(data != nullptr || data_size == 0, "DrawCommandList write texture data is null");

        void* memory = command_list.allocate(CalculateSize(data_size), alignof(WriteTextureCommand));
        auto* command = new (memory) WriteTextureCommand(texture, mip_level, array_slice, row_pitch, data_size);
        if (data_size > 0) {
            std::memcpy(command->mutableData(), data, data_size);
        }

        command_list.appendCommand(command);
        return *command;
    }

    const void* DrawCommandList::WriteTextureCommand::data() const {
        return reinterpret_cast<const UInt8*>(this) + sizeof(WriteTextureCommand);
    }

    DrawCommandList::WriteTextureCommand::WriteTextureCommand(
        const GfxTextureHandle& texture,
        UInt32 mip_level,
        UInt32 array_slice,
        Size_t row_pitch,
        Size_t data_size)
        : DrawCommand(CalculateSize(data_size), &WriteTextureCommand::ExecuteCommand, &WriteTextureCommand::DestroyCommand),
          m_texture(texture),
          m_mip_level(mip_level),
          m_array_slice(array_slice),
          m_row_pitch(row_pitch),
          m_data_size(data_size) {}

    void DrawCommandList::WriteTextureCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const WriteTextureCommand&>(command).execute(command_list);
    }

    void DrawCommandList::WriteTextureCommand::DestroyCommand(DrawCommand& command) {
        static_cast<WriteTextureCommand&>(command).~WriteTextureCommand();
    }

    Size_t DrawCommandList::WriteTextureCommand::CalculateSize(Size_t data_size) {
        return sizeof(WriteTextureCommand) + data_size;
    }

    void* DrawCommandList::WriteTextureCommand::mutableData() {
        return reinterpret_cast<UInt8*>(this) + sizeof(WriteTextureCommand);
    }

    void DrawCommandList::WriteTextureCommand::execute(GfxCommandList& command_list) const {
        command_list.writeTexture(m_texture->getRHIHandle(), m_mip_level, m_array_slice, data(), m_row_pitch);
    }

    DrawCommandList::PushConstantsCommand& DrawCommandList::PushConstantsCommand::Create(
        DrawCommandList& command_list,
        const void* data,
        Size_t byte_size)
    {
        DO_ASSERT(data != nullptr || byte_size == 0, "DrawCommandList push constants data is null");

        void* memory = command_list.allocate(CalculateSize(byte_size), alignof(PushConstantsCommand));
        auto* command = new (memory) PushConstantsCommand(byte_size);
        if (byte_size > 0) {
            std::memcpy(command->mutableData(), data, byte_size);
        }

        command_list.appendCommand(command);
        return *command;
    }

    const void* DrawCommandList::PushConstantsCommand::data() const {
        return reinterpret_cast<const UInt8*>(this) + sizeof(PushConstantsCommand);
    }

    DrawCommandList::PushConstantsCommand::PushConstantsCommand(Size_t byte_size)
        : DrawCommand(CalculateSize(byte_size), &PushConstantsCommand::ExecuteCommand, &PushConstantsCommand::DestroyCommand),
          m_byte_size(byte_size) {}

    void DrawCommandList::PushConstantsCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const PushConstantsCommand&>(command).execute(command_list);
    }

    void DrawCommandList::PushConstantsCommand::DestroyCommand(DrawCommand& command) {
        static_cast<PushConstantsCommand&>(command).~PushConstantsCommand();
    }

    Size_t DrawCommandList::PushConstantsCommand::CalculateSize(Size_t byte_size) {
        return sizeof(PushConstantsCommand) + byte_size;
    }

    void* DrawCommandList::PushConstantsCommand::mutableData() {
        return reinterpret_cast<UInt8*>(this) + sizeof(PushConstantsCommand);
    }

    void DrawCommandList::PushConstantsCommand::execute(GfxCommandList& command_list) const {
        command_list.setPushConstants(data(), m_byte_size);
    }

    DrawCommandList::SetTextureStateCommand::SetTextureStateCommand(
        const GfxTextureHandle& texture,
        const GfxTextureSubresourceSet& subresources,
        GfxResourceStates state_bits)
        : m_texture(texture), m_subresources(subresources), m_state_bits(state_bits) {}

    void DrawCommandList::SetTextureStateCommand::execute(GfxCommandList& command_list) const {
        command_list.setTextureState(m_texture->getRHIHandle(), m_subresources, m_state_bits);
    }

    DrawCommandList::SetBufferStateCommand::SetBufferStateCommand(const GfxBufferHandle& buffer, GfxResourceStates state_bits)
        : m_buffer(buffer), m_state_bits(state_bits) {}

    void DrawCommandList::SetBufferStateCommand::execute(GfxCommandList& command_list) const {
        command_list.setBufferState(m_buffer->getRHIHandle(), m_state_bits);
    }

    void DrawCommandList::CommitBarriersCommand::execute(GfxCommandList& command_list) const {
        command_list.commitBarriers();
    }

    DrawCommandList::BeginMarkerCommand& DrawCommandList::BeginMarkerCommand::Create(DrawCommandList& command_list, const char* name) {
        const char* marker_name = name != nullptr ? name : "";
        Size_t name_length = std::strlen(marker_name);

        void* memory = command_list.allocate(CalculateSize(name_length), alignof(BeginMarkerCommand));
        auto* command = new (memory) BeginMarkerCommand(name_length);
        std::memcpy(command->mutableName(), marker_name, name_length + 1);

        command_list.appendCommand(command);
        return *command;
    }

    const char* DrawCommandList::BeginMarkerCommand::name() const {
        return reinterpret_cast<const char*>(this + 1);
    }

    DrawCommandList::BeginMarkerCommand::BeginMarkerCommand(Size_t name_length)
        : DrawCommand(CalculateSize(name_length), &BeginMarkerCommand::ExecuteCommand, &BeginMarkerCommand::DestroyCommand) {}

    void DrawCommandList::BeginMarkerCommand::ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
        static_cast<const BeginMarkerCommand&>(command).execute(command_list);
    }

    void DrawCommandList::BeginMarkerCommand::DestroyCommand(DrawCommand& command) {
        static_cast<BeginMarkerCommand&>(command).~BeginMarkerCommand();
    }

    Size_t DrawCommandList::BeginMarkerCommand::CalculateSize(Size_t name_length) {
        return sizeof(BeginMarkerCommand) + name_length + 1;
    }

    char* DrawCommandList::BeginMarkerCommand::mutableName() {
        return reinterpret_cast<char*>(this + 1);
    }

    void DrawCommandList::BeginMarkerCommand::execute(GfxCommandList& command_list) const {
        command_list.beginMarker(name());
    }

    void DrawCommandList::EndMarkerCommand::execute(GfxCommandList& command_list) const {
        command_list.endMarker();
    }

    Size_t DrawCommandList::alignUp(Size_t value, Size_t alignment) {
        DO_ASSERT(alignment != 0, "DrawCommandList alignment is zero");
        DO_ASSERT((alignment & (alignment - 1)) == 0, "DrawCommandList alignment is invalid");
        Size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    void DrawCommandList::appendCommand(DrawCommandList::DrawCommand* command) {
        DO_ASSERT(command != nullptr, "DrawCommandList command is null");

        command->m_next = nullptr;
        if (m_tail != nullptr) {
            m_tail->m_next = command;
        }
        else {
            m_head = command;
        }

        m_tail = command;
        ++m_command_count;
    }

    void DrawCommandList::moveFrom(DrawCommandList&& other) {
        m_head = other.m_head;
        m_tail = other.m_tail;
        m_command_count = other.m_command_count;

        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_command_count = 0;
    }

    void* DrawCommandList::allocate(Size_t size, Size_t alignment) {
        DO_ASSERT(size > 0, "DrawCommandList allocate size is zero");
        DO_ASSERT(alignment <= alignof(std::max_align_t), "DrawCommandList alignment exceeds max alignment");
        return Memory::AllocateFrame(size, alignment, AllocTag::RenderCmd);
    }

} // dodoe
