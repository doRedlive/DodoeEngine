// do@Redlive

#include "draw_command_list.h"

namespace dodoe {

    DrawCommandList::DrawCommand::DrawCommand(Size_t size, ExecuteFunction execute, DestroyFunction destroy)
        : m_size(size), m_execute(execute), m_destroy(destroy) {}

    DrawCommandList::DrawCommandList() : DrawCommandList(kDefaultBlockSize) {}

    DrawCommandList::DrawCommandList(Size_t default_block_size)
        : m_default_block_size(default_block_size) {
        DO_ASSERT(m_default_block_size > 0, "DrawCommandList block size is zero");
    }

    DrawCommandList::~DrawCommandList() {
        reset();
        releaseBlocks();
    }

    DrawCommandList::DrawCommandList(DrawCommandList&& other) noexcept {
        moveFrom(std::move(other));
    }

    DrawCommandList& DrawCommandList::operator=(DrawCommandList&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();
        releaseBlocks();
        moveFrom(std::move(other));
        return *this;
    }

    void DrawCommandList::reset() {
        DrawCommand* command = m_head;
        while (command != nullptr) {
            DrawCommand* next = command->m_next;
            DO_ASSERT(command->m_destroy != nullptr, "DrawCommandList destroy function is null");
            command->m_destroy(*command);
            command = next;
        }

        m_head = nullptr;
        m_tail = nullptr;
        m_command_count = 0;
        m_used_byte_size = 0;

        for (auto& block : m_blocks) {
            block.m_offset = 0;
        }
    }

    void DrawCommandList::reserve(Size_t byte_size) {
        if (byte_size == 0) {
            return;
        }

        if (!m_blocks.empty()) {
            MemoryBlock& block = m_blocks.back();
            if (block.m_size - block.m_offset >= byte_size) {
                return;
            }
        }

        createBlock(byte_size);
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
        m_used_byte_size += other.m_used_byte_size;

        for (auto& block : other.m_blocks) {
            m_blocks.push_back(std::move(block));
        }

        other.m_blocks.clear();
        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_command_count = 0;
        other.m_used_byte_size = 0;
    }

    void DrawCommandList::execute(gfx::ICommandList& command_list) const {
        const DrawCommand* command = m_head;
        while (command != nullptr) {
            DO_ASSERT(command->m_execute != nullptr, "DrawCommandList execute function is null");
            command->m_execute(*command, command_list);
            command = command->m_next;
        }
    }

    void DrawCommandList::execute(const gfx::CommandListHandle& command_list) const {
        DO_ASSERT(command_list != nullptr, "DrawCommandList command list is null");
        execute(*command_list);
    }

    Bool DrawCommandList::isEmpty() const {
        return m_head == nullptr;
    }

    Size_t DrawCommandList::commandCount() const {
        return m_command_count;
    }

    Size_t DrawCommandList::usedByteSize() const {
        return m_used_byte_size;
    }

    Size_t DrawCommandList::blockCount() const {
        return m_blocks.size();
    }

    Size_t DrawCommandList::defaultBlockSize() const {
        return m_default_block_size;
    }

    void DrawCommandList::open() {
        enqueue<OpenCommand>();
    }

    void DrawCommandList::close() {
        enqueue<CloseCommand>();
    }

    void DrawCommandList::clearState() {
        enqueue<ClearStateCommand>();
    }

    void DrawCommandList::beginMarker(const char* name) {
        BeginMarkerCommand::Create(*this, name);
    }

    void DrawCommandList::endMarker() {
        enqueue<EndMarkerCommand>();
    }

    void DrawCommandList::clearTextureFloat(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        const gfx::Color& clear_color)
    {
        enqueue<ClearTextureFloatCommand>(texture, subresources, clear_color);
    }

    void DrawCommandList::clearTextureUInt(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        UInt32 clear_color)
    {
        enqueue<ClearTextureUIntCommand>(texture, subresources, clear_color);
    }

    void DrawCommandList::clearDepthStencilTexture(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        Bool clear_depth,
        Float depth,
        Bool clear_stencil,
        UInt8 stencil)
    {
        enqueue<ClearDepthStencilTextureCommand>(texture, subresources, clear_depth, depth, clear_stencil, stencil);
    }

    void DrawCommandList::copyBuffer(
        const gfx::BufferHandle& destination,
        UInt64 destination_offset_bytes,
        const gfx::BufferHandle& source,
        UInt64 source_offset_bytes,
        UInt64 data_size_bytes)
    {
        enqueue<CopyBufferCommand>(destination, destination_offset_bytes, source, source_offset_bytes, data_size_bytes);
    }

    void DrawCommandList::writeBuffer(
        const gfx::BufferHandle& buffer,
        const void* data,
        Size_t data_size,
        UInt64 destination_offset_bytes)
    {
        WriteBufferCommand::Create(*this, buffer, data, data_size, destination_offset_bytes);
    }

    void DrawCommandList::setPushConstants(const void* data, Size_t byte_size) {
        PushConstantsCommand::Create(*this, data, byte_size);
    }

    void DrawCommandList::setTextureState(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        gfx::ResourceStates state_bits)
    {
        enqueue<SetTextureStateCommand>(texture, subresources, state_bits);
    }

    void DrawCommandList::commitBarriers() {
        enqueue<CommitBarriersCommand>();
    }

    void DrawCommandList::setGraphicsState(const gfx::GraphicsState& state) {
        enqueue<SetGraphicsStateCommand>(state);
    }

    void DrawCommandList::setComputeState(const gfx::ComputeState& state) {
        enqueue<SetComputeStateCommand>(state);
    }

    void DrawCommandList::draw(const gfx::DrawArguments& args) {
        enqueue<DrawPrimitiveCommand>(args);
    }

    void DrawCommandList::drawIndexed(const gfx::DrawArguments& args) {
        enqueue<DrawIndexedPrimitiveCommand>(args);
    }

    void DrawCommandList::dispatch(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z) {
        enqueue<DispatchCommand>(groups_x, groups_y, groups_z);
    }

    DrawCommandList::ClearTextureFloatCommand::ClearTextureFloatCommand(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        const gfx::Color& clear_color)
        : m_texture(texture), m_subresources(subresources), m_clear_color(clear_color) {}

    void DrawCommandList::ClearTextureFloatCommand::execute(gfx::ICommandList& command_list) const {
        command_list.clearTextureFloat(m_texture, m_subresources, m_clear_color);
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

    void DrawCommandList::BeginMarkerCommand::ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list) {
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

    void DrawCommandList::BeginMarkerCommand::execute(gfx::ICommandList& command_list) const {
        command_list.beginMarker(name());
    }

    void DrawCommandList::EndMarkerCommand::execute(gfx::ICommandList& command_list) const {
        command_list.endMarker();
    }

    DrawCommandList::ClearTextureUIntCommand::ClearTextureUIntCommand(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        UInt32 clear_color)
        : m_texture(texture), m_subresources(subresources), m_clear_color(clear_color) {}

    void DrawCommandList::ClearTextureUIntCommand::execute(gfx::ICommandList& command_list) const {
        command_list.clearTextureUInt(m_texture, m_subresources, m_clear_color);
    }

    DrawCommandList::ClearDepthStencilTextureCommand::ClearDepthStencilTextureCommand(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
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

    void DrawCommandList::ClearDepthStencilTextureCommand::execute(gfx::ICommandList& command_list) const {
        command_list.clearDepthStencilTexture(m_texture, m_subresources, m_clear_depth, m_depth, m_clear_stencil, m_stencil);
    }

    DrawCommandList::CopyBufferCommand::CopyBufferCommand(
        const gfx::BufferHandle& destination,
        UInt64 destination_offset_bytes,
        const gfx::BufferHandle& source,
        UInt64 source_offset_bytes,
        UInt64 data_size_bytes)
        : m_destination(destination),
          m_source(source),
          m_destination_offset_bytes(destination_offset_bytes),
          m_source_offset_bytes(source_offset_bytes),
          m_data_size_bytes(data_size_bytes) {}

    void DrawCommandList::CopyBufferCommand::execute(gfx::ICommandList& command_list) const {
        command_list.copyBuffer(m_destination, m_destination_offset_bytes, m_source, m_source_offset_bytes, m_data_size_bytes);
    }

    DrawCommandList::WriteBufferCommand& DrawCommandList::WriteBufferCommand::Create(
        DrawCommandList& command_list,
        const gfx::BufferHandle& buffer,
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
        const gfx::BufferHandle& buffer,
        UInt64 destination_offset_bytes,
        Size_t data_size)
        : DrawCommand(CalculateSize(data_size), &WriteBufferCommand::ExecuteCommand, &WriteBufferCommand::DestroyCommand),
          m_buffer(buffer),
          m_destination_offset_bytes(destination_offset_bytes),
          m_data_size(data_size) {}

    void DrawCommandList::WriteBufferCommand::ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list) {
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

    void DrawCommandList::WriteBufferCommand::execute(gfx::ICommandList& command_list) const {
        command_list.writeBuffer(m_buffer, data(), m_data_size, m_destination_offset_bytes);
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

    void DrawCommandList::PushConstantsCommand::ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list) {
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

    void DrawCommandList::PushConstantsCommand::execute(gfx::ICommandList& command_list) const {
        command_list.setPushConstants(data(), m_byte_size);
    }

    DrawCommandList::SetTextureStateCommand::SetTextureStateCommand(
        const gfx::TextureHandle& texture,
        const gfx::TextureSubresourceSet& subresources,
        gfx::ResourceStates state_bits)
        : m_texture(texture), m_subresources(subresources), m_state_bits(state_bits) {}

    void DrawCommandList::SetTextureStateCommand::execute(gfx::ICommandList& command_list) const {
        command_list.setTextureState(m_texture, m_subresources, m_state_bits);
    }

    void DrawCommandList::CommitBarriersCommand::execute(gfx::ICommandList& command_list) const {
        command_list.commitBarriers();
    }

    DrawCommandList::SetGraphicsStateCommand::SetGraphicsStateCommand(const gfx::GraphicsState& state)
        : m_state(state) {}

    void DrawCommandList::SetGraphicsStateCommand::execute(gfx::ICommandList& command_list) const {
        command_list.setGraphicsState(m_state);
    }

    DrawCommandList::SetComputeStateCommand::SetComputeStateCommand(const gfx::ComputeState& state)
        : m_state(state) {}

    void DrawCommandList::SetComputeStateCommand::execute(gfx::ICommandList& command_list) const {
        command_list.setComputeState(m_state);
    }

    DrawCommandList::DrawPrimitiveCommand::DrawPrimitiveCommand(const gfx::DrawArguments& args)
        : m_args(args) {}

    void DrawCommandList::DrawPrimitiveCommand::execute(gfx::ICommandList& command_list) const {
        command_list.draw(m_args);
    }

    DrawCommandList::DrawIndexedPrimitiveCommand::DrawIndexedPrimitiveCommand(const gfx::DrawArguments& args)
        : m_args(args) {}

    void DrawCommandList::DrawIndexedPrimitiveCommand::execute(gfx::ICommandList& command_list) const {
        command_list.drawIndexed(m_args);
    }

    DrawCommandList::DispatchCommand::DispatchCommand(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z)
        : m_groups_x(groups_x), m_groups_y(groups_y), m_groups_z(groups_z) {}

    void DrawCommandList::DispatchCommand::execute(gfx::ICommandList& command_list) const {
        command_list.dispatch(m_groups_x, m_groups_y, m_groups_z);
    }

    void DrawCommandList::OpenCommand::execute(gfx::ICommandList& command_list) const {
        command_list.open();
    }

    void DrawCommandList::CloseCommand::execute(gfx::ICommandList& command_list) const {
        command_list.close();
    }

    void DrawCommandList::ClearStateCommand::execute(gfx::ICommandList& command_list) const {
        command_list.clearState();
    }

    Size_t DrawCommandList::alignUp(Size_t value, Size_t alignment) {
        DO_ASSERT(alignment != 0, "DrawCommandList alignment is zero");
        DO_ASSERT((alignment & (alignment - 1)) == 0, "DrawCommandList alignment is invalid");
        Size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    void DrawCommandList::appendCommand(DrawCommand* command) {
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
        m_blocks = std::move(other.m_blocks);
        m_head = other.m_head;
        m_tail = other.m_tail;
        m_command_count = other.m_command_count;
        m_used_byte_size = other.m_used_byte_size;
        m_default_block_size = other.m_default_block_size;

        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_command_count = 0;
        other.m_used_byte_size = 0;
        other.m_default_block_size = kDefaultBlockSize;
    }

    void DrawCommandList::releaseBlocks() {
        m_blocks.clear();
    }

    void DrawCommandList::createBlock(Size_t minimum_size) {
        DO_ASSERT(minimum_size > 0, "DrawCommandList block size is zero");
        Size_t block_size = std::max(m_default_block_size, minimum_size);

        MemoryBlock block{};
        block.m_data = Scope<UInt8[]>{new UInt8[block_size]};
        block.m_size = block_size;
        block.m_offset = 0;

        m_blocks.push_back(std::move(block));
    }

    void* DrawCommandList::allocate(Size_t size, Size_t alignment) {
        DO_ASSERT(size > 0, "DrawCommandList allocate size is zero");
        DO_ASSERT(alignment <= alignof(std::max_align_t), "DrawCommandList alignment exceeds max alignment");

        if (m_blocks.empty()) {
            createBlock(size + alignment);
        }

        MemoryBlock* block = &m_blocks.back();
        Size_t aligned_offset = alignUp(block->m_offset, alignment);

        if (aligned_offset + size > block->m_size) {
            createBlock(size + alignment);
            block = &m_blocks.back();
            aligned_offset = alignUp(block->m_offset, alignment);
        }

        void* memory = block->m_data.get() + aligned_offset;
        block->m_offset = aligned_offset + size;
        m_used_byte_size += size;
        return memory;
    }

} // dodoe
