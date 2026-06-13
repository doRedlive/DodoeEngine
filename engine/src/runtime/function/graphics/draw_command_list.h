// do@Redlive

#pragma once

#include "dopch.h"

#include "gfx.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace dodoe {

    class DrawCommandList {
        struct MemoryBlock {
            Scope<UInt8[]> m_data{};
            Size_t m_size{0};
            Size_t m_offset{0};
        };

        DynamicArray<MemoryBlock> m_blocks{};
        struct DrawCommand* m_head{nullptr};
        struct DrawCommand* m_tail{nullptr};
        Size_t m_command_count{0};
        Size_t m_used_byte_size{0};
        Size_t m_default_block_size{4096};

    public:
        inline static constexpr Size_t kDefaultBlockSize = 4096;

        struct DrawCommand {
            using ExecuteFunction = void(*)(const DrawCommand&, gfx::ICommandList&);
            using DestroyFunction = void(*)(DrawCommand&);

            DrawCommand* m_next{nullptr};
            Size_t m_size{0};
            ExecuteFunction m_execute{nullptr};
            DestroyFunction m_destroy{nullptr};

            DrawCommand(Size_t size, ExecuteFunction execute, DestroyFunction destroy);
        };

        template <typename TDerived>
        struct Command : DrawCommand {
            Command() : DrawCommand(sizeof(TDerived), &Command::ExecuteCommand, &Command::DestroyCommand) {}

        private:
            static void ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list) {
                static_cast<const TDerived&>(command).execute(command_list);
            }

            static void DestroyCommand(DrawCommand& command) {
                static_cast<TDerived&>(command).~TDerived();
            }
        };

        DrawCommandList();
        explicit DrawCommandList(Size_t default_block_size);
        ~DrawCommandList();

        DrawCommandList(const DrawCommandList&) = delete;
        DrawCommandList& operator=(const DrawCommandList&) = delete;

        DrawCommandList(DrawCommandList&& other) noexcept;
        DrawCommandList& operator=(DrawCommandList&& other) noexcept;

        template <typename TCommand, typename... TArgs>
        TCommand& enqueue(TArgs&&... args) {
            static_assert(std::is_base_of_v<DrawCommand, TCommand>);
            static_assert(alignof(TCommand) <= alignof(std::max_align_t));

            void* memory = allocate(sizeof(TCommand), alignof(TCommand));
            auto* command = new (memory) TCommand(std::forward<TArgs>(args)...);
            appendCommand(command);
            return *command;
        }

        void reset();
        void reserve(Size_t byte_size);
        void append(DrawCommandList&& other);
        void execute(gfx::ICommandList& command_list) const;
        void execute(const gfx::CommandListHandle& command_list) const;

        [[nodiscard]] Bool isEmpty() const;
        [[nodiscard]] Size_t commandCount() const;
        [[nodiscard]] Size_t usedByteSize() const;
        [[nodiscard]] Size_t blockCount() const;
        [[nodiscard]] Size_t defaultBlockSize() const;

        void open();
        void close();
        void clearState();
        void beginMarker(const char* name);
        void endMarker();
        void clearTextureFloat(const gfx::TextureHandle& texture, const gfx::TextureSubresourceSet& subresources, const gfx::Color& clear_color);
        void clearTextureUInt(const gfx::TextureHandle& texture, const gfx::TextureSubresourceSet& subresources, UInt32 clear_color);
        void clearDepthStencilTexture(
            const gfx::TextureHandle& texture,
            const gfx::TextureSubresourceSet& subresources,
            Bool clear_depth,
            Float depth,
            Bool clear_stencil,
            UInt8 stencil);
        void copyBuffer(
            const gfx::BufferHandle& destination,
            UInt64 destination_offset_bytes,
            const gfx::BufferHandle& source,
            UInt64 source_offset_bytes,
            UInt64 data_size_bytes);
        void writeBuffer(const gfx::BufferHandle& buffer, const void* data, Size_t data_size, UInt64 destination_offset_bytes = 0);

        template <typename TData>
        void writeBuffer(const gfx::BufferHandle& buffer, const TData& data, UInt64 destination_offset_bytes = 0) {
            static_assert(std::is_trivially_copyable_v<TData>);
            writeBuffer(buffer, std::addressof(data), sizeof(TData), destination_offset_bytes);
        }

        void setPushConstants(const void* data, Size_t byte_size);

        template <typename TData>
        void setPushConstants(const TData& data) {
            static_assert(std::is_trivially_copyable_v<TData>);
            setPushConstants(std::addressof(data), sizeof(TData));
        }

        void setTextureState(const gfx::TextureHandle& texture, const gfx::TextureSubresourceSet& subresources, gfx::ResourceStates state_bits);
        void commitBarriers();
        void setGraphicsState(const gfx::GraphicsState& state);
        void setComputeState(const gfx::ComputeState& state);
        void draw(const gfx::DrawArguments& args);
        void drawIndexed(const gfx::DrawArguments& args);
        void dispatch(UInt32 groups_x, UInt32 groups_y = 1, UInt32 groups_z = 1);

    private:
        struct OpenCommand final : Command<OpenCommand> {
            OpenCommand() = default;
            void execute(gfx::ICommandList& command_list) const;
        };

        struct CloseCommand final : Command<CloseCommand> {
            CloseCommand() = default;
            void execute(gfx::ICommandList& command_list) const;
        };

        struct ClearStateCommand final : Command<ClearStateCommand> {
            ClearStateCommand() = default;
            void execute(gfx::ICommandList& command_list) const;
        };

        struct BeginMarkerCommand final : DrawCommand {
            static BeginMarkerCommand& Create(DrawCommandList& command_list, const char* name);

            [[nodiscard]] const char* name() const;

        private:
            explicit BeginMarkerCommand(Size_t name_length);

            static void ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t name_length);
            [[nodiscard]] char* mutableName();
            void execute(gfx::ICommandList& command_list) const;
        };

        struct EndMarkerCommand final : Command<EndMarkerCommand> {
            EndMarkerCommand() = default;
            void execute(gfx::ICommandList& command_list) const;
        };

        struct ClearTextureFloatCommand final : Command<ClearTextureFloatCommand> {
            gfx::TextureHandle m_texture{};
            gfx::TextureSubresourceSet m_subresources{};
            gfx::Color m_clear_color{};

            ClearTextureFloatCommand(const gfx::TextureHandle& texture, const gfx::TextureSubresourceSet& subresources, const gfx::Color& clear_color);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct ClearTextureUIntCommand final : Command<ClearTextureUIntCommand> {
            gfx::TextureHandle m_texture{};
            gfx::TextureSubresourceSet m_subresources{};
            UInt32 m_clear_color{0};

            ClearTextureUIntCommand(const gfx::TextureHandle& texture, const gfx::TextureSubresourceSet& subresources, UInt32 clear_color);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct ClearDepthStencilTextureCommand final : Command<ClearDepthStencilTextureCommand> {
            gfx::TextureHandle m_texture{};
            gfx::TextureSubresourceSet m_subresources{};
            Float m_depth{1.0f};
            Bool m_clear_depth{true};
            Bool m_clear_stencil{false};
            UInt8 m_stencil{0};

            ClearDepthStencilTextureCommand(
                const gfx::TextureHandle& texture,
                const gfx::TextureSubresourceSet& subresources,
                Bool clear_depth,
                Float depth,
                Bool clear_stencil,
                UInt8 stencil);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct CopyBufferCommand final : Command<CopyBufferCommand> {
            gfx::BufferHandle m_destination{};
            gfx::BufferHandle m_source{};
            UInt64 m_destination_offset_bytes{0};
            UInt64 m_source_offset_bytes{0};
            UInt64 m_data_size_bytes{0};

            CopyBufferCommand(
                const gfx::BufferHandle& destination,
                UInt64 destination_offset_bytes,
                const gfx::BufferHandle& source,
                UInt64 source_offset_bytes,
                UInt64 data_size_bytes);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct WriteBufferCommand final : DrawCommand {
            gfx::BufferHandle m_buffer{};
            UInt64 m_destination_offset_bytes{0};
            Size_t m_data_size{0};

            static WriteBufferCommand& Create(
                DrawCommandList& command_list,
                const gfx::BufferHandle& buffer,
                const void* data,
                Size_t data_size,
                UInt64 destination_offset_bytes);

            [[nodiscard]] const void* data() const;

        private:
            WriteBufferCommand(const gfx::BufferHandle& buffer, UInt64 destination_offset_bytes, Size_t data_size);

            static void ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t data_size);
            [[nodiscard]] void* mutableData();
            void execute(gfx::ICommandList& command_list) const;
        };

        struct PushConstantsCommand final : DrawCommand {
            Size_t m_byte_size{0};

            static PushConstantsCommand& Create(DrawCommandList& command_list, const void* data, Size_t byte_size);

            [[nodiscard]] const void* data() const;

        private:
            explicit PushConstantsCommand(Size_t byte_size);

            static void ExecuteCommand(const DrawCommand& command, gfx::ICommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t byte_size);
            [[nodiscard]] void* mutableData();
            void execute(gfx::ICommandList& command_list) const;
        };

        struct SetTextureStateCommand final : Command<SetTextureStateCommand> {
            gfx::TextureHandle m_texture{};
            gfx::TextureSubresourceSet m_subresources{};
            gfx::ResourceStates m_state_bits{gfx::ResourceStates::Unknown};

            SetTextureStateCommand(
                const gfx::TextureHandle& texture,
                const gfx::TextureSubresourceSet& subresources,
                gfx::ResourceStates state_bits);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct CommitBarriersCommand final : Command<CommitBarriersCommand> {
            CommitBarriersCommand() = default;
            void execute(gfx::ICommandList& command_list) const;
        };

        struct SetGraphicsStateCommand final : Command<SetGraphicsStateCommand> {
            gfx::GraphicsState m_state{};

            explicit SetGraphicsStateCommand(const gfx::GraphicsState& state);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct SetComputeStateCommand final : Command<SetComputeStateCommand> {
            gfx::ComputeState m_state{};

            explicit SetComputeStateCommand(const gfx::ComputeState& state);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct DrawPrimitiveCommand final : Command<DrawPrimitiveCommand> {
            gfx::DrawArguments m_args{};

            explicit DrawPrimitiveCommand(const gfx::DrawArguments& args);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct DrawIndexedPrimitiveCommand final : Command<DrawIndexedPrimitiveCommand> {
            gfx::DrawArguments m_args{};

            explicit DrawIndexedPrimitiveCommand(const gfx::DrawArguments& args);
            void execute(gfx::ICommandList& command_list) const;
        };

        struct DispatchCommand final : Command<DispatchCommand> {
            UInt32 m_groups_x{1};
            UInt32 m_groups_y{1};
            UInt32 m_groups_z{1};

            DispatchCommand(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z);
            void execute(gfx::ICommandList& command_list) const;
        };

        [[nodiscard]] static Size_t alignUp(Size_t value, Size_t alignment);
        void appendCommand(DrawCommand* command);
        void moveFrom(DrawCommandList&& other);
        void releaseBlocks();
        void createBlock(Size_t minimum_size);
        [[nodiscard]] void* allocate(Size_t size, Size_t alignment);
    };

} // dodoe
