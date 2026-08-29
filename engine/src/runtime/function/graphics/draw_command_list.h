// do@Redlive

#pragma once

#include "dopch.h"

#include "gfx.h"
#include "runtime/core/container/command_list.h"
#include "runtime/core/memory/allocator.h"

#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace dodoe {

    using DescriptorIndex = int;

    class GfxContext;

    class DrawCommandList : public CommandList<GfxCommandList> {
    public:
        using CommandList::execute;

        inline static constexpr Size_t kDefaultBlockSize = 4096;

        DrawCommandList() = default;

    private:
        GfxDeviceHandle m_device{};
        std::mutex m_record_mutex{};

        Deque<GfxCommandListHandle> m_upload_list_pool{};
        std::mutex m_upload_pool_mutex{};

        [[nodiscard]] GfxCommandListHandle acquireUploadCommandList();
        void releaseUploadCommandList(GfxCommandListHandle& command_list);

    public:
        void setDevice(GfxDeviceHandle device) { m_device = device; }
        void setDevice(class GfxContext& gfx);
        [[nodiscard]] GfxDeviceHandle getDevice() const { return m_device; }

        template <typename TCommand, typename... TArgs>
        TCommand& recordCommand(TArgs&&... args) {
            std::lock_guard<std::mutex> lock(m_record_mutex);
            return enqueue<TCommand>(std::forward<TArgs>(args)...);
        }

        CommandList<GfxCommandList> detachRecordedCommands();
        void beginFrame();
        void endFrame();

        void execute(const GfxCommandListHandle& command_list) const;

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

        template <typename TData, typename = std::enable_if_t<!std::is_pointer_v<TData>>>
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
        void drawIndirect(UInt32 offset_bytes, UInt32 draw_count = 1);
        void drawIndexedIndirect(UInt32 offset_bytes, UInt32 draw_count = 1);
        void dispatch(UInt32 groups_x, UInt32 groups_y = 1, UInt32 groups_z = 1);
        void dispatchIndirect(UInt32 offset_bytes);

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
#define GFX_DRAW_CMD(Name) struct Name final : CommandImpl<Name>

        template <typename TDerived>
        struct VarCmd : Command {
        protected:
            explicit VarCmd(Size_t size) : Command(size, &SExec, &SDestroy) {}
        public:
            void* mutableData() { return (UInt8*)this + sizeof(TDerived); }
            const void* data() const { return (const UInt8*)this + sizeof(TDerived); }
            static Size_t CalcSize(Size_t s) { return alignUp(sizeof(TDerived) + s, alignof(TDerived)); }
            template <typename... A>
            static TDerived& Create(DrawCommandList& cl, Size_t ds, const void* d, A&&... a) {
                std::lock_guard<std::mutex> lock(cl.m_record_mutex);
                void* m = cl.allocate(CalcSize(ds), alignof(TDerived));
                auto* c = new (m) TDerived(std::forward<A>(a)..., ds);
                if (d && ds > 0) std::memcpy(c->mutableData(), d, ds);
                cl.appendCommand(c);
                return *c;
            }
        private:
            static void SExec(const Command& c, GfxCommandList& e) { static_cast<const TDerived&>(c).execute(e); }
            static void SDestroy(Command& c) { static_cast<TDerived&>(c).~TDerived(); }
        };

        GFX_DRAW_CMD(OpenCommand) { OpenCommand() = default; void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(CloseCommand) { CloseCommand() = default; void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(ClearStateCommand) { ClearStateCommand() = default; void execute(GfxCommandList&) const; };

        GFX_DRAW_CMD(EndMarkerCommand) { EndMarkerCommand() = default; void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(ClearTextureFloatCommand) { GfxTextureHandle m_texture{}; GfxTextureSubresourceSet m_subresources{}; GfxColor m_clear_color{}; ClearTextureFloatCommand(const GfxTextureHandle& t, const GfxTextureSubresourceSet& s, const GfxColor& c) : m_texture(t), m_subresources(s), m_clear_color(c) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(ClearTextureUIntCommand) { GfxTextureHandle m_texture{}; GfxTextureSubresourceSet m_subresources{}; UInt32 m_clear_color{0}; ClearTextureUIntCommand(const GfxTextureHandle& t, const GfxTextureSubresourceSet& s, UInt32 c) : m_texture(t), m_subresources(s), m_clear_color(c) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(ClearDepthStencilTextureCommand) { GfxTextureHandle m_texture{}; GfxTextureSubresourceSet m_subresources{}; Float m_depth{1}; Bool m_clear_depth{true}, m_clear_stencil{false}; UInt8 m_stencil{0}; ClearDepthStencilTextureCommand(const GfxTextureHandle& t, const GfxTextureSubresourceSet& s, Bool cd, Float d, Bool cs, UInt8 st) : m_texture(t), m_subresources(s), m_depth(d), m_clear_depth(cd), m_clear_stencil(cs), m_stencil(st) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(CopyBufferCommand) { GfxBufferHandle m_dst{}, m_src{}; UInt64 m_dst_off{0}, m_src_off{0}, m_size{0}; CopyBufferCommand(const GfxBufferHandle& d, UInt64 doff, const GfxBufferHandle& s, UInt64 soff, UInt64 sz) : m_dst(d), m_src(s), m_dst_off(doff), m_src_off(soff), m_size(sz) {} void execute(GfxCommandList&) const; };

        struct WriteBufferCommand final : VarCmd<WriteBufferCommand> {
            GfxBufferHandle m_b{}; UInt64 m_off{0}; Size_t m_sz{0};
            WriteBufferCommand(const GfxBufferHandle& b, UInt64 o, Size_t s) : VarCmd(CalcSize(s)), m_b(b), m_off(o), m_sz(s) {}
            static WriteBufferCommand& Create(DrawCommandList& cl, const GfxBufferHandle& b, const void* d, Size_t s, UInt64 o) {
                auto& c = VarCmd::Create(cl, s, d, b, o); return c;
            }
            void execute(GfxCommandList& c) const { if (m_b->isRHIReady()) c.writeBuffer(m_b->getRHIHandle(), data(), m_sz, m_off); }
        };

        struct WriteTextureCommand final : VarCmd<WriteTextureCommand> {
            GfxTextureHandle m_t{}; UInt32 m_m{0}, m_s{0}; Size_t m_p{0}, m_ds{0};
            WriteTextureCommand(const GfxTextureHandle& tx, UInt32 mi, UInt32 sl, Size_t pi, Size_t sz) : VarCmd(CalcSize(sz)), m_t(tx), m_m(mi), m_s(sl), m_p(pi), m_ds(sz) {}
            static WriteTextureCommand& Create(DrawCommandList& cl, const GfxTextureHandle& tx, UInt32 mi, UInt32 sl, const void* d, Size_t pi, Size_t sz) { auto& c=VarCmd::Create(cl,sz,d,tx,mi,sl,pi); return c; }
            void execute(GfxCommandList& c) const { if (m_t->isRHIReady()) c.writeTexture(m_t->getRHIHandle(), m_m, m_s, data(), m_p); }
        };

        struct PushConstantsCommand final : VarCmd<PushConstantsCommand> {
            Size_t m_s{0};
            explicit PushConstantsCommand(Size_t sz) : VarCmd(CalcSize(sz)), m_s(sz) {}
            static PushConstantsCommand& Create(DrawCommandList& cl, const void* d, Size_t sz) { auto& c=VarCmd::Create(cl,sz,d); return c; }
            void execute(GfxCommandList& c) const { c.setPushConstants(data(), m_s); }
        };

        struct BeginMarkerCommand final : VarCmd<BeginMarkerCommand> {
            static BeginMarkerCommand& Create(DrawCommandList& cl, const char* name) { const char* n=name?name:""; Size_t l=std::strlen(n)+1; auto& c=VarCmd::Create(cl,l,(const void*)n); return c; }
            const char* name() const { return (const char*)data(); }
            void execute(GfxCommandList& c) const { c.beginMarker(name()); }
            friend class VarCmd<BeginMarkerCommand>;
        private: explicit BeginMarkerCommand(Size_t l) : VarCmd(CalcSize(l)) {}
        };

        GFX_DRAW_CMD(SetTextureStateCommand) { GfxTextureHandle m_t{}; GfxTextureSubresourceSet m_s{}; GfxResourceStates m_st{GfxResourceStates::Unknown}; SetTextureStateCommand(const GfxTextureHandle& t, const GfxTextureSubresourceSet& s, GfxResourceStates st) : m_t(t), m_s(s), m_st(st) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(SetBufferStateCommand) { GfxBufferHandle m_b{}; GfxResourceStates m_st{GfxResourceStates::Unknown}; SetBufferStateCommand(const GfxBufferHandle& b, GfxResourceStates s) : m_b(b), m_st(s) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(CommitBarriersCommand) { CommitBarriersCommand() = default; void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DrawPrimitiveCommand) { GfxDrawArguments m_args{}; explicit DrawPrimitiveCommand(const GfxDrawArguments& a) : m_args(a) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DrawIndexedPrimitiveCommand) { GfxDrawArguments m_args{}; explicit DrawIndexedPrimitiveCommand(const GfxDrawArguments& a) : m_args(a) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DispatchCommand) { UInt32 m_x{1},m_y{1},m_z{1}; DispatchCommand(UInt32 x,UInt32 y,UInt32 z) : m_x(x),m_y(y),m_z(z) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DrawIndirectCommand) { UInt32 m_off{0},m_cnt{1}; DrawIndirectCommand(UInt32 o,UInt32 c) : m_off(o),m_cnt(c) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DrawIndexedIndirectCommand) { UInt32 m_off{0},m_cnt{1}; DrawIndexedIndirectCommand(UInt32 o,UInt32 c) : m_off(o),m_cnt(c) {} void execute(GfxCommandList&) const; };
        GFX_DRAW_CMD(DispatchIndirectCommand) { UInt32 m_off{0}; explicit DispatchIndirectCommand(UInt32 o) : m_off(o) {} void execute(GfxCommandList&) const; };

        struct SetGraphicsStateCommand final : CommandImpl<SetGraphicsStateCommand> {
            GfxFramebufferHandle m_fb{}; GfxGraphicsPipelineHandle m_pso{}; DynamicArray<GfxBindingSetHandle> m_bs{}; GfxViewportState m_vp{}; DynamicArray<GfxVertexBufferBinding> m_vb{}; GfxIndexBufferBinding m_ib{};
            SetGraphicsStateCommand(const GfxFramebufferHandle& fb, const GfxGraphicsPipelineHandle& pso, const DynamicArray<GfxBindingSetHandle>& bs, const GfxViewportState& vp, const DynamicArray<GfxVertexBufferBinding>& vb, const GfxIndexBufferBinding& ib);
            void execute(GfxCommandList&) const;
        };

        GFX_DRAW_CMD(SetComputeStateCommand) { GfxComputeState m_st{}; explicit SetComputeStateCommand(const GfxComputeState& s) : m_st(s) {} void execute(GfxCommandList&) const; };

        GFX_DRAW_CMD(SetGraphicsStateByValueCommand) { GfxGraphicsState m_st{}; explicit SetGraphicsStateByValueCommand(const GfxGraphicsState& s) : m_st(s) {} void execute(GfxCommandList& c) const { c.setGraphicsState(m_st); } };

        [[nodiscard]] static Size_t alignUp(Size_t value, Size_t alignment);
    };

    extern DrawCommandList GDrawCommandList;

} // dodoe
