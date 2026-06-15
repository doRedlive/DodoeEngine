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
        Size_t m_command_count{0};
        Size_t m_used_byte_size{0};
        Size_t m_default_block_size{4096};

    public:
        inline static constexpr Size_t kDefaultBlockSize = 4096;

        struct DrawCommand {
            using ExecuteFunction = void(*)(const DrawCommand&, GfxCommandList&);
            using DestroyFunction = void(*)(DrawCommand&);

            DrawCommand* m_next{nullptr};
            Size_t m_size{0};
            ExecuteFunction m_execute{nullptr};
            DestroyFunction m_destroy{nullptr};

            DrawCommand(Size_t size, ExecuteFunction execute, DestroyFunction destroy);
        };

    private:
        DrawCommand* m_head{nullptr};
        DrawCommand* m_tail{nullptr};

    public:

        template <typename TDerived>
        struct Command : DrawCommand {
            Command() : DrawCommand(sizeof(TDerived), &Command::ExecuteCommand, &Command::DestroyCommand) {}

        private:
            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list) {
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
        void execute(GfxCommandList& command_list) const;
        void execute(const GfxCommandListHandle& command_list) const;

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
        void clearTextureFloat(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color);
        void clearTextureUInt(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, UInt32 clear_color);
        void clearDepthStencilTexture(
            const GfxTextureHandle& texture,
            const GfxTextureSubresourceSet& subresources,
            Bool clear_depth,
            Float depth,
            Bool clear_stencil,
            UInt8 stencil);
        void copyBuffer(
            const GfxBufferHandle& destination,
            UInt64 destination_offset_bytes,
            const GfxBufferHandle& source,
            UInt64 source_offset_bytes,
            UInt64 data_size_bytes);
        void writeBuffer(const GfxBufferHandle& buffer, const void* data, Size_t data_size, UInt64 destination_offset_bytes = 0);
        void createBuffer(GfxDeviceHandle device, const GfxBufferDesc& desc, GfxBufferHandle* destination, const void* data, Size_t data_size);

        template <typename TData>
        void writeBuffer(const GfxBufferHandle& buffer, const TData& data, UInt64 destination_offset_bytes = 0) {
            static_assert(std::is_trivially_copyable_v<TData>);
            writeBuffer(buffer, std::addressof(data), sizeof(TData), destination_offset_bytes);
        }

        void setPushConstants(const void* data, Size_t byte_size);

        template <typename TData>
        void setPushConstants(const TData& data) {
            static_assert(std::is_trivially_copyable_v<TData>);
            setPushConstants(std::addressof(data), sizeof(TData));
        }

        void setTextureState(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, GfxResourceStates state_bits);
        void setBufferState(const GfxBufferHandle& buffer, GfxResourceStates state_bits);
        void commitBarriers();
        void setGraphicsState(const GfxGraphicsState& state);
        void setComputeState(const GfxComputeState& state);
        void draw(const GfxDrawArguments& args);
        void drawIndexed(const GfxDrawArguments& args);
        void dispatch(UInt32 groups_x, UInt32 groups_y = 1, UInt32 groups_z = 1);

    private:
        struct OpenCommand final : Command<OpenCommand> {
            OpenCommand() = default;
            void execute(GfxCommandList& command_list) const;
        };

        struct CloseCommand final : Command<CloseCommand> {
            CloseCommand() = default;
            void execute(GfxCommandList& command_list) const;
        };

        struct ClearStateCommand final : Command<ClearStateCommand> {
            ClearStateCommand() = default;
            void execute(GfxCommandList& command_list) const;
        };

        struct BeginMarkerCommand final : DrawCommand {
            static BeginMarkerCommand& Create(DrawCommandList& command_list, const char* name);

            [[nodiscard]] const char* name() const;

        private:
            explicit BeginMarkerCommand(Size_t name_length);

            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t name_length);
            [[nodiscard]] char* mutableName();
            void execute(GfxCommandList& command_list) const;
        };

        struct EndMarkerCommand final : Command<EndMarkerCommand> {
            EndMarkerCommand() = default;
            void execute(GfxCommandList& command_list) const;
        };

        struct ClearTextureFloatCommand final : Command<ClearTextureFloatCommand> {
            GfxTextureHandle m_texture{};
            GfxTextureSubresourceSet m_subresources{};
            GfxColor m_clear_color{};

            ClearTextureFloatCommand(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color);
            void execute(GfxCommandList& command_list) const;
        };

        struct ClearTextureUIntCommand final : Command<ClearTextureUIntCommand> {
            GfxTextureHandle m_texture{};
            GfxTextureSubresourceSet m_subresources{};
            UInt32 m_clear_color{0};

            ClearTextureUIntCommand(const GfxTextureHandle& texture, const GfxTextureSubresourceSet& subresources, UInt32 clear_color);
            void execute(GfxCommandList& command_list) const;
        };

        struct ClearDepthStencilTextureCommand final : Command<ClearDepthStencilTextureCommand> {
            GfxTextureHandle m_texture{};
            GfxTextureSubresourceSet m_subresources{};
            Float m_depth{1.0f};
            Bool m_clear_depth{true};
            Bool m_clear_stencil{false};
            UInt8 m_stencil{0};

            ClearDepthStencilTextureCommand(
                const GfxTextureHandle& texture,
                const GfxTextureSubresourceSet& subresources,
                Bool clear_depth,
                Float depth,
                Bool clear_stencil,
                UInt8 stencil);
            void execute(GfxCommandList& command_list) const;
        };

        struct CopyBufferCommand final : Command<CopyBufferCommand> {
            GfxBufferHandle m_destination{};
            GfxBufferHandle m_source{};
            UInt64 m_destination_offset_bytes{0};
            UInt64 m_source_offset_bytes{0};
            UInt64 m_data_size_bytes{0};

            CopyBufferCommand(
                const GfxBufferHandle& destination,
                UInt64 destination_offset_bytes,
                const GfxBufferHandle& source,
                UInt64 source_offset_bytes,
                UInt64 data_size_bytes);
            void execute(GfxCommandList& command_list) const;
        };

        struct WriteBufferCommand final : DrawCommand {
            GfxBufferHandle m_buffer{};
            UInt64 m_destination_offset_bytes{0};
            Size_t m_data_size{0};

            static WriteBufferCommand& Create(
                DrawCommandList& command_list,
                const GfxBufferHandle& buffer,
                const void* data,
                Size_t data_size,
                UInt64 destination_offset_bytes);

            [[nodiscard]] const void* data() const;

        private:
            WriteBufferCommand(const GfxBufferHandle& buffer, UInt64 destination_offset_bytes, Size_t data_size);

            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t data_size);
            [[nodiscard]] void* mutableData();
            void execute(GfxCommandList& command_list) const;
        };

        struct PushConstantsCommand final : DrawCommand {
            Size_t m_byte_size{0};

            static PushConstantsCommand& Create(DrawCommandList& command_list, const void* data, Size_t byte_size);

            [[nodiscard]] const void* data() const;

        private:
            explicit PushConstantsCommand(Size_t byte_size);

            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t byte_size);
            [[nodiscard]] void* mutableData();
            void execute(GfxCommandList& command_list) const;
        };

        struct SetTextureStateCommand final : Command<SetTextureStateCommand> {
            GfxTextureHandle m_texture{};
            GfxTextureSubresourceSet m_subresources{};
            GfxResourceStates m_state_bits{GfxResourceStates::Unknown};

            SetTextureStateCommand(
                const GfxTextureHandle& texture,
                const GfxTextureSubresourceSet& subresources,
                GfxResourceStates state_bits);
            void execute(GfxCommandList& command_list) const;
        };

        struct SetBufferStateCommand final : Command<SetBufferStateCommand> {
            GfxBufferHandle m_buffer{};
            GfxResourceStates m_state_bits{GfxResourceStates::Unknown};

            SetBufferStateCommand(const GfxBufferHandle& buffer, GfxResourceStates state_bits);
            void execute(GfxCommandList& command_list) const;
        };

        struct CommitBarriersCommand final : Command<CommitBarriersCommand> {
            CommitBarriersCommand() = default;
            void execute(GfxCommandList& command_list) const;
        };

        struct SetGraphicsStateCommand final : Command<SetGraphicsStateCommand> {
            GfxGraphicsState m_state{};

            explicit SetGraphicsStateCommand(const GfxGraphicsState& state);
            void execute(GfxCommandList& command_list) const;
        };

        struct SetComputeStateCommand final : Command<SetComputeStateCommand> {
            GfxComputeState m_state{};

            explicit SetComputeStateCommand(const GfxComputeState& state);
            void execute(GfxCommandList& command_list) const;
        };

        struct DrawPrimitiveCommand final : Command<DrawPrimitiveCommand> {
            GfxDrawArguments m_args{};

            explicit DrawPrimitiveCommand(const GfxDrawArguments& args);
            void execute(GfxCommandList& command_list) const;
        };

        struct DrawIndexedPrimitiveCommand final : Command<DrawIndexedPrimitiveCommand> {
            GfxDrawArguments m_args{};

            explicit DrawIndexedPrimitiveCommand(const GfxDrawArguments& args);
            void execute(GfxCommandList& command_list) const;
        };

        struct DispatchCommand final : Command<DispatchCommand> {
            UInt32 m_groups_x{1};
            UInt32 m_groups_y{1};
            UInt32 m_groups_z{1};

            DispatchCommand(UInt32 groups_x, UInt32 groups_y, UInt32 groups_z);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateBufferCommand final : DrawCommand {
            GfxDeviceHandle m_device{};
            GfxBufferDesc m_desc{};
            GfxBufferHandle* m_destination{};
            Size_t m_data_size{0};

            static CreateBufferCommand& Create(
                DrawCommandList& command_list,
                GfxDeviceHandle device,
                const GfxBufferDesc& desc,
                GfxBufferHandle* destination,
                const void* data,
                Size_t data_size);

            [[nodiscard]] const void* data() const;

        private:
            CreateBufferCommand(GfxDeviceHandle device, const GfxBufferDesc& desc, GfxBufferHandle* destination, Size_t data_size);

            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t data_size);
            [[nodiscard]] void* mutableData();
            void execute(GfxCommandList& command_list) const;
        };

        [[nodiscard]] static Size_t alignUp(Size_t value, Size_t alignment);
        void appendCommand(DrawCommand* command);
        void moveFrom(DrawCommandList&& other);
        void releaseBlocks();
        void createBlock(Size_t minimum_size);
        [[nodiscard]] void* allocate(Size_t size, Size_t alignment);
    };

} // dodoe
