// do@Redlive

#pragma once

#include "dopch.h"

#include "gfx.h"

#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace dodoe {

    using DescriptorIndex = int;

    class GfxContext;

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
        GfxDeviceHandle m_device{};
        GfxCommandList* m_immediate_target{nullptr};
        std::recursive_mutex m_enqueue_mutex{};

    public:
        void beginImmediateFrame(GfxCommandList& cmd) { m_immediate_target = &cmd; }
        void endImmediateFrame() { m_immediate_target = nullptr; }
        [[nodiscard]] bool isImmediate() const { return m_immediate_target != nullptr; }

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

            std::lock_guard<std::recursive_mutex> lock(m_enqueue_mutex);
            void* memory = allocate(sizeof(TCommand), alignof(TCommand));
            auto* command = new (memory) TCommand(std::forward<TArgs>(args)...);
            appendCommand(command);
            return *command;
        }

        void setDevice(GfxDeviceHandle device) { m_device = device; }
        void setDevice(class GfxContext& gfx);
        [[nodiscard]] GfxDeviceHandle getDevice() const { return m_device; }

        void beginFrame();
        void endFrame();

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

        // ── Deferred draw / state commands ──────────────────────

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
        void writeTexture(const GfxTextureHandle& texture, UInt32 mip_level, UInt32 array_slice, const void* data, Size_t row_pitch);

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
        void setGraphicsState(
            const GfxFramebufferHandle& framebuffer,
            const GfxGraphicsPipelineHandle& pipeline,
            const DynamicArray<GfxBindingSetHandle>& binding_sets,
            const GfxViewportState& viewport,
            const DynamicArray<GfxVertexBufferBinding>& vertex_buffers = {},
            const GfxIndexBufferBinding& index_buffer = {});
        void setGraphicsState(const GfxGraphicsState& state);
        void setComputeState(const GfxComputeState& state);
        void draw(const GfxDrawArguments& args);
        void drawIndexed(const GfxDrawArguments& args);
        void dispatch(UInt32 groups_x, UInt32 groups_y = 1, UInt32 groups_z = 1);

        GfxTextureHandle createTexture(const GfxTextureDesc& desc, const void* data = nullptr, Size_t data_size = 0);
        GfxBufferHandle createBuffer(const GfxBufferDesc& desc, const void* data = nullptr, Size_t data_size = 0);
        GfxFramebufferHandle createFramebuffer(const GfxFramebufferDesc& desc);
        GfxBindingSetHandle createBindingSet(const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout);
        GfxGraphicsPipelineHandle createGraphicsPipeline(const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& framebuffer_info);
        DescriptorIndex createDescriptor(GfxDescriptorTableHandle descriptor_table, GfxTextureHandle texture, UInt32 slot);

        GfxSamplerHandle createSampler(const GfxSamplerDesc& desc);
        GfxBindingLayoutHandle createBindingLayout(const GfxBindingLayoutDesc& desc);
        GfxInputLayoutHandle createInputLayout(const GfxVertexAttributeDesc* attributes, UInt32 count, GfxShaderHandle shader);
        GfxShaderHandle createShader(const GfxShaderDesc& desc, const void* data, Size_t data_size);

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

        struct WriteTextureCommand final : DrawCommand {
            GfxTextureHandle m_texture{};
            UInt32 m_mip_level{0};
            UInt32 m_array_slice{0};
            Size_t m_row_pitch{0};
            Size_t m_data_size{0};

            static WriteTextureCommand& Create(
                DrawCommandList& command_list,
                const GfxTextureHandle& texture,
                UInt32 mip_level,
                UInt32 array_slice,
                const void* data,
                Size_t row_pitch,
                Size_t data_size);

            [[nodiscard]] const void* data() const;

        private:
            WriteTextureCommand(const GfxTextureHandle& texture, UInt32 mip_level, UInt32 array_slice, Size_t row_pitch, Size_t data_size);

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

        struct SetGraphicsStateByValueCommand final : Command<SetGraphicsStateByValueCommand> {
            GfxGraphicsState m_state{};
            explicit SetGraphicsStateByValueCommand(const GfxGraphicsState& state) : m_state(state) {}
            void execute(GfxCommandList& command_list) const { command_list.setGraphicsState(m_state); }
        };

        struct SetGraphicsStateCommand final : Command<SetGraphicsStateCommand> {
            GfxFramebufferHandle m_framebuffer{};
            GfxGraphicsPipelineHandle m_pipeline{};
            DynamicArray<GfxBindingSetHandle> m_binding_sets{};
            GfxViewportState m_viewport{};
            DynamicArray<GfxVertexBufferBinding> m_vertex_buffers{};
            GfxIndexBufferBinding m_index_buffer{};

            SetGraphicsStateCommand(
                const GfxFramebufferHandle& framebuffer,
                const GfxGraphicsPipelineHandle& pipeline,
                const DynamicArray<GfxBindingSetHandle>& binding_sets,
                const GfxViewportState& viewport,
                const DynamicArray<GfxVertexBufferBinding>& vertex_buffers,
                const GfxIndexBufferBinding& index_buffer);
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

        // ── Deferred resource creation commands ──────────────────────

        struct CreateTextureCommand final : Command<CreateTextureCommand> {
            GfxDeviceHandle m_device{};
            GfxTextureDesc m_desc{};
            GfxTextureHandle m_texture{};
            Size_t m_data_size{0};

            CreateTextureCommand(GfxDeviceHandle device, const GfxTextureDesc& desc, GfxTextureHandle texture, const void* data, Size_t data_size);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateBufferCommand final : Command<CreateBufferCommand> {
            GfxDeviceHandle m_device{};
            GfxBufferDesc m_desc{};
            GfxBufferHandle m_buffer{};
            Size_t m_data_size{0};

            static CreateBufferCommand& Create(
                DrawCommandList& command_list,
                GfxDeviceHandle device,
                const GfxBufferDesc& desc,
                GfxBufferHandle buffer,
                const void* data,
                Size_t data_size);
            [[nodiscard]] const void* data() const;
            void execute(GfxCommandList& command_list) const;

        private:
            CreateBufferCommand(GfxDeviceHandle device, const GfxBufferDesc& desc, GfxBufferHandle buffer, Size_t data_size);
            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t data_size);
            [[nodiscard]] void* mutableData();
        };

        struct CreateFramebufferCommand final : Command<CreateFramebufferCommand> {
            GfxDeviceHandle m_device{};
            GfxFramebufferDesc m_desc{};
            GfxFramebufferHandle m_framebuffer{};

            CreateFramebufferCommand(GfxDeviceHandle device, const GfxFramebufferDesc& desc, GfxFramebufferHandle framebuffer);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateBindingSetCommand final : Command<CreateBindingSetCommand> {
            GfxDeviceHandle m_device{};
            GfxBindingSetDesc m_desc{};
            GfxBindingLayoutHandle m_layout{};
            GfxBindingSetHandle m_binding_set{};

            CreateBindingSetCommand(GfxDeviceHandle device, const GfxBindingSetDesc& desc, const GfxBindingLayoutHandle& layout, GfxBindingSetHandle binding_set);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateGraphicsPipelineCommand final : Command<CreateGraphicsPipelineCommand> {
            GfxDeviceHandle m_device{};
            GfxGraphicsPipelineDesc m_desc{};
            GfxFramebufferInfo m_framebuffer_info{};
            GfxGraphicsPipelineHandle m_pipeline{};

            CreateGraphicsPipelineCommand(GfxDeviceHandle device, const GfxGraphicsPipelineDesc& desc, const GfxFramebufferInfo& framebuffer_info, GfxGraphicsPipelineHandle pipeline);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateDescriptorCommand final : Command<CreateDescriptorCommand> {
            GfxDeviceHandle m_device{};
            GfxDescriptorTableHandle m_descriptor_table{};
            GfxTextureHandle m_texture{};
            DescriptorIndex m_destination{0};
            UInt32 m_slot{};

            CreateDescriptorCommand(GfxDeviceHandle device, GfxDescriptorTableHandle descriptor_table, GfxTextureHandle texture, UInt32 slot);
            void execute(GfxCommandList& command_list) const;
        };

        struct CreateShaderCommand final : DrawCommand {
            GfxDeviceHandle m_device{};
            GfxShaderDesc m_desc{};
            GfxShaderHandle m_shader{};
            Size_t m_data_size{0};

            static CreateShaderCommand& Create(
                DrawCommandList& command_list,
                GfxDeviceHandle device,
                const GfxShaderDesc& desc,
                GfxShaderHandle shader,
                const void* data,
                Size_t data_size);
            [[nodiscard]] const void* data() const;
            void execute(GfxCommandList& command_list) const;

        private:
            CreateShaderCommand(GfxDeviceHandle device, const GfxShaderDesc& desc, GfxShaderHandle shader, Size_t data_size);
            static void ExecuteCommand(const DrawCommand& command, GfxCommandList& command_list);
            static void DestroyCommand(DrawCommand& command);
            [[nodiscard]] static Size_t CalculateSize(Size_t data_size);
            [[nodiscard]] void* mutableData();
        };

        [[nodiscard]] static Size_t alignUp(Size_t value, Size_t alignment);
        void appendCommand(DrawCommand* command);
        void moveFrom(DrawCommandList&& other);
        void releaseBlocks();
        void createBlock(Size_t minimum_size);
        [[nodiscard]] void* allocate(Size_t size, Size_t alignment);
    };

    struct ImmediateFrameScope {
        GfxDeviceHandle m_device;
        GfxContext* m_gfx;
        GfxCommandListHandle m_cmd;
        UInt32 m_image_index;

        ImmediateFrameScope(GfxDeviceHandle device, GfxContext* gfx, UInt32 image_index);
        ~ImmediateFrameScope();

        void flush();

        ImmediateFrameScope(const ImmediateFrameScope&) = delete;
        ImmediateFrameScope& operator=(const ImmediateFrameScope&) = delete;
    };

    extern DrawCommandList GDrawCommandList;

} // dodoe
