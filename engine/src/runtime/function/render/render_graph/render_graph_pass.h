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
    class SharedRenderService;
    class ShaderLibrary;
    class PipelineStateCache;
    class TextureManager;
    class FrameStagingAllocator;

    struct RenderGraphExecuteContext {
        const RenderViewFamily* view_family{nullptr};
        const RenderScene* scene{nullptr};
        const RenderView* view{nullptr};
        GfxContext* gfx_context{nullptr};
        SharedRenderService* shared_render_service{nullptr};
        FrameStagingAllocator* frame_staging_allocator{nullptr};
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
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_execute_context->shared_render_service; }
        [[nodiscard]] FrameStagingAllocator* getFrameStagingAllocator() const { return m_execute_context->frame_staging_allocator; }

        [[nodiscard]] const ShaderLibrary* getShaderLibrary() const;
        [[nodiscard]] PipelineStateCache* getPipelineStateCache() const;
        [[nodiscard]] TextureManager* getTextureManager() const;

        [[nodiscard]] GfxTextureHandle resolveTexture(const RenderGraphTextureHandle handle) const {
            DO_ASSERT(m_resource_registry != nullptr, "RenderGraphPassContext resource registry is null");
            return m_resource_registry->getTexture(handle);
        }

        [[nodiscard]] GfxBufferHandle resolveBuffer(const RenderGraphBufferHandle handle) const {
            DO_ASSERT(m_resource_registry != nullptr, "RenderGraphPassContext resource registry is null");
            return m_resource_registry->getBuffer(handle);
        }
    };

    class RenderGraphPass {
        String m_name{};
        RenderGraphPassFlags m_flags{RenderGraphPassFlags::None};
        DynamicArray<RenderGraphPassResourceAccess> m_accesses{};
        DynamicArray<RenderGraphBarrier> m_pre_barriers{};
        std::function<void(const RenderGraphPassContext&, DrawCommandList&)> m_execute_function{};
        UInt32 m_subgraph_index{~0u};

    public:
        RenderGraphPass() = default;
        RenderGraphPass(const String& name, const RenderGraphPassFlags flags) : m_name(name), m_flags(flags) { }

        void addAccess(const UInt32 resource_index, const RenderGraphAccessType access_type,
                       const RenderGraphPipelineStage stage = RenderGraphPipelineStage::PixelShader,
                       const RenderGraphSubresourceRange& subresource = {}) {
            RenderGraphPassResourceAccess access{};
            access.resource_index = resource_index;
            access.access_type = access_type;
            access.stage = stage;
            access.subresource = subresource;
            m_accesses.push_back(access);
        }

        void setSubgraphIndex(const UInt32 index) { m_subgraph_index = index; }
        [[nodiscard]] UInt32 getSubgraphIndex() const { return m_subgraph_index; }

        void setExecuteFunction(std::function<void(const RenderGraphPassContext&, DrawCommandList&)>&& execute_function) {
            m_execute_function = std::move(execute_function);
        }

        void execute(const RenderGraphPassContext& context, DrawCommandList& command_list) const {
            DO_ASSERT(m_execute_function, "RenderGraphPass execute function is empty");
            m_execute_function(context, command_list);
        }

        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] RenderGraphPassFlags getFlags() const { return m_flags; }
        [[nodiscard]] const DynamicArray<RenderGraphPassResourceAccess>& getAccesses() const { return m_accesses; }
        [[nodiscard]] const DynamicArray<RenderGraphBarrier>& getPreBarriers() const { return m_pre_barriers; }
        void setAutoBarriers(DynamicArray<RenderGraphBarrier>&& barriers) { m_pre_barriers = std::move(barriers); }
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

        RenderGraphTextureHandle readTexture(const RenderGraphTextureHandle handle,
                                             const RenderGraphPipelineStage stage,
                                             const RenderGraphSubresourceRange& subresource = {});
        RenderGraphTextureHandle writeColor(const RenderGraphTextureHandle handle,
                                            const RenderGraphAttachmentInfo& attachment = {});
        RenderGraphTextureHandle writeDepth(const RenderGraphTextureHandle handle,
                                            const RenderGraphAttachmentInfo& attachment = {});
        RenderGraphTextureHandle writeUav(const RenderGraphTextureHandle handle,
                                          const RenderGraphPipelineStage stage);
        RenderGraphBufferHandle readBuffer(const RenderGraphBufferHandle handle,
                                           const RenderGraphPipelineStage stage);
        RenderGraphBufferHandle writeBuffer(const RenderGraphBufferHandle handle,
                                            const RenderGraphPipelineStage stage);
        void exportTexture(const RenderGraphTextureHandle handle, const GfxResourceStates final_state);

        RenderGraphTextureHandle read(const RenderGraphTextureHandle handle);
        RenderGraphTextureHandle write(const RenderGraphTextureHandle handle);
        RenderGraphBufferHandle read(const RenderGraphBufferHandle handle);
        RenderGraphBufferHandle write(const RenderGraphBufferHandle handle);

        [[nodiscard]] RenderGraphBlackboard& blackboard() const;
    };

} // dodoe
