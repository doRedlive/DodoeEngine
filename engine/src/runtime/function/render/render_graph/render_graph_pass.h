// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/function/graphics/gfx.h"

#include "render_graph_blackboard.h"
#include "render_graph_resource.h"
#include "render_graph_resource_registry.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class RenderGraphBuilder;

    enum class RenderGraphPassFlags : UInt32 {
        None = 0,
        Raster = 1 << 0,
        Compute = 1 << 1,
        Copy = 1 << 2,
        NeverCull = 1 << 3,
        AsyncCompute = 1 << 4,
    };

    inline RenderGraphPassFlags operator|(const RenderGraphPassFlags lhs, const RenderGraphPassFlags rhs) {
        return static_cast<RenderGraphPassFlags>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline RenderGraphPassFlags operator&(const RenderGraphPassFlags lhs, const RenderGraphPassFlags rhs) {
        return static_cast<RenderGraphPassFlags>(static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs));
    }

    inline Bool HasAnyFlags(const RenderGraphPassFlags lhs, const RenderGraphPassFlags rhs) {
        return static_cast<UInt32>(lhs & rhs) != 0;
    }

    class RenderScene;
    class RenderView;
    class RenderViewFamily;

    struct RenderGraphExecuteContext {
        const RenderViewFamily* view_family{nullptr};
        const RenderScene* scene{nullptr};
        const RenderView* view{nullptr};
        GfxContext* gfx_context{nullptr};
        Size_t view_index{0};
        UInt32 swapchain_image_index{0};
    };

    class RenderGraphPassContext {
        const RenderGraphExecuteContext* m_execute_context{nullptr};
        const RenderGraphResourceRegistry* m_resource_registry{nullptr};

    public:
        RenderGraphPassContext(const RenderGraphExecuteContext& execute_context, const RenderGraphResourceRegistry& resource_registry)
            : m_execute_context(&execute_context), m_resource_registry(&resource_registry) { }

        [[nodiscard]] const RenderViewFamily* getViewFamily() const { return m_execute_context->view_family; }
        [[nodiscard]] const RenderScene* getScene() const { return m_execute_context->scene; }
        [[nodiscard]] const RenderView* getView() const { return m_execute_context->view; }
        [[nodiscard]] GfxContext* getGfxContext() const { return m_execute_context->gfx_context; }
        [[nodiscard]] Size_t getViewIndex() const { return m_execute_context->view_index; }
        [[nodiscard]] UInt32 getSwapchainImageIndex() const { return m_execute_context->swapchain_image_index; }

        [[nodiscard]] GfxTextureHandle resolveTexture(const RenderGraphTextureHandle handle) const {
            DO_ASSERT(m_resource_registry != nullptr, "RenderGraphPassContext resource registry is null");
            return m_resource_registry->getTexture(handle);
        }

        [[nodiscard]] GfxBufferHandle resolveBuffer(const RenderGraphBufferHandle handle) const {
            DO_ASSERT(m_resource_registry != nullptr, "RenderGraphPassContext resource registry is null");
            return m_resource_registry->getBuffer(handle);
        }

    };

    class RenderGraphCommandList {
        const RenderGraphPassContext* m_pass_context{nullptr};
        DrawCommandList* m_draw_command_list{nullptr};

    public:
        RenderGraphCommandList(const RenderGraphPassContext& pass_context, DrawCommandList& draw_command_list)
            : m_pass_context(&pass_context), m_draw_command_list(&draw_command_list) { }

        [[nodiscard]] GfxTextureHandle resolveTexture(const RenderGraphTextureHandle handle) const { return m_pass_context->resolveTexture(handle); }
        [[nodiscard]] GfxBufferHandle resolveBuffer(const RenderGraphBufferHandle handle) const { return m_pass_context->resolveBuffer(handle); }
        void open() const { m_draw_command_list->open(); }
        void close() const { m_draw_command_list->close(); }
        void clearState() const { m_draw_command_list->clearState(); }
        void beginMarker(const char* name) const { m_draw_command_list->beginMarker(name); }
        void endMarker() const { m_draw_command_list->endMarker(); }
        void clearTextureFloat(const RenderGraphTextureHandle handle, const GfxTextureSubresourceSet& subresources, const GfxColor& clear_color) const {
            m_draw_command_list->clearTextureFloat(resolveTexture(handle), subresources, clear_color);
        }
        void clearTextureUInt(const RenderGraphTextureHandle handle, const GfxTextureSubresourceSet& subresources, const UInt32 clear_color) const {
            m_draw_command_list->clearTextureUInt(resolveTexture(handle), subresources, clear_color);
        }
        void clearDepthStencilTexture(
            const RenderGraphTextureHandle handle,
            const GfxTextureSubresourceSet& subresources,
            const Bool clear_depth,
            const Float depth,
            const Bool clear_stencil,
            const UInt8 stencil) const
        {
            m_draw_command_list->clearDepthStencilTexture(resolveTexture(handle), subresources, clear_depth, depth, clear_stencil, stencil);
        }
        void copyBuffer(
            const RenderGraphBufferHandle destination,
            const UInt64 destination_offset_bytes,
            const RenderGraphBufferHandle source,
            const UInt64 source_offset_bytes,
            const UInt64 data_size_bytes) const
        {
            m_draw_command_list->copyBuffer(resolveBuffer(destination), destination_offset_bytes, resolveBuffer(source), source_offset_bytes, data_size_bytes);
        }
        void writeBuffer(const RenderGraphBufferHandle buffer, const void* data, const Size_t data_size, const UInt64 destination_offset_bytes = 0) const {
            m_draw_command_list->writeBuffer(resolveBuffer(buffer), data, data_size, destination_offset_bytes);
        }
        void setPushConstants(const void* data, const Size_t byte_size) const { m_draw_command_list->setPushConstants(data, byte_size); }
        void setTextureState(
            const RenderGraphTextureHandle handle,
            const GfxTextureSubresourceSet& subresources,
            const GfxResourceStates state_bits) const
        {
            m_draw_command_list->setTextureState(resolveTexture(handle), subresources, state_bits);
        }
        void setBufferState(const RenderGraphBufferHandle buffer, const GfxResourceStates state_bits) const {
            m_draw_command_list->setBufferState(resolveBuffer(buffer), state_bits);
        }
        void commitBarriers() const { m_draw_command_list->commitBarriers(); }
        void setGraphicsState(const GfxGraphicsState& state) const { m_draw_command_list->setGraphicsState(state); }
        void setComputeState(const GfxComputeState& state) const { m_draw_command_list->setComputeState(state); }
        void draw(const GfxDrawArguments& args) const { m_draw_command_list->draw(args); }
        void drawIndexed(const GfxDrawArguments& args) const { m_draw_command_list->drawIndexed(args); }
        void dispatch(const UInt32 groups_x, const UInt32 groups_y = 1, const UInt32 groups_z = 1) const { m_draw_command_list->dispatch(groups_x, groups_y, groups_z); }
    };

    class RenderGraphPass {
        String m_name{};
        RenderGraphPassFlags m_flags{RenderGraphPassFlags::None};
        DynamicArray<RenderGraphPassResourceAccess> m_accesses{};
        std::function<void(const RenderGraphPassContext&, RenderGraphCommandList&)> m_execute_function{};

    public:
        RenderGraphPass() = default;
        RenderGraphPass(const String& name, const RenderGraphPassFlags flags) : m_name(name), m_flags(flags) { }

        void addAccess(const UInt32 resource_index, const RenderGraphAccessType access_type) {
            RenderGraphPassResourceAccess access{};
            access.resource_index = resource_index;
            access.access_type = access_type;
            m_accesses.push_back(access);
        }

        void setExecuteFunction(std::function<void(const RenderGraphPassContext&, RenderGraphCommandList&)>&& execute_function) {
            m_execute_function = std::move(execute_function);
        }

        void execute(const RenderGraphPassContext& context, RenderGraphCommandList& command_list) const {
            DO_ASSERT(m_execute_function, "RenderGraphPass execute function is empty");
            m_execute_function(context, command_list);
        }

        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] RenderGraphPassFlags getFlags() const { return m_flags; }
        [[nodiscard]] const DynamicArray<RenderGraphPassResourceAccess>& getAccesses() const { return m_accesses; }
    };

    class RenderGraphPassBuilder {
        RenderGraphBuilder* m_builder{nullptr};
        RenderGraphPass* m_pass{nullptr};

    public:
        RenderGraphPassBuilder(RenderGraphBuilder& builder, RenderGraphPass& pass) : m_builder(&builder), m_pass(&pass) { }

        RenderGraphTextureHandle createTransientTexture(const RenderGraphTextureDesc& desc, const String& name);
        RenderGraphBufferHandle createTransientBuffer(const RenderGraphBufferDesc& desc, const String& name);
        RenderGraphTextureHandle importTexture(const GfxTextureHandle& texture, const String& name);
        RenderGraphBufferHandle importBuffer(const GfxBufferHandle& buffer, const String& name);
        RenderGraphTextureHandle importBackBuffer(const String& name);

        RenderGraphTextureHandle read(const RenderGraphTextureHandle handle);
        RenderGraphTextureHandle write(const RenderGraphTextureHandle handle);
        RenderGraphBufferHandle read(const RenderGraphBufferHandle handle);
        RenderGraphBufferHandle write(const RenderGraphBufferHandle handle);

        [[nodiscard]] RenderGraphBlackboard& blackboard() const;
    };

} // dodoe
