// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_blackboard.h"
#include "render_graph.h"

namespace dodoe {

    class RenderGraphBuilder {
        RenderGraph m_graph{};
        RenderGraphBlackboard m_blackboard{};

    public:
        RenderGraphBuilder() = default;

        RenderGraphTextureHandle createTexture(const RenderGraphTextureDesc& desc, const String& name);
        RenderGraphBufferHandle createBuffer(const RenderGraphBufferDesc& desc, const String& name);
        RenderGraphTextureHandle importTexture(const GfxTextureHandle& texture, const String& name);
        RenderGraphBufferHandle importBuffer(const GfxBufferHandle& buffer, const String& name);
        RenderGraphTextureHandle importBackBuffer(const String& name);

        template <typename TParameters, typename TSetup, typename TExecute>
        void addPass(const String& name, const RenderGraphPassFlags flags, TSetup&& setup_function, TExecute&& execute_function) {
            auto pass = create_ref<RenderGraphPass>(name, flags);
            TParameters parameters{};
            RenderGraphPassBuilder pass_builder(*this, *pass);
            setup_function(pass_builder, parameters);
            pass->setExecuteFunction(
                [parameters = std::move(parameters), execute = std::forward<TExecute>(execute_function)](
                    const RenderGraphPassContext& context,
                    DrawCommandList& command_list) mutable
                {
                    execute(parameters, context, command_list);
                }
            );
            m_graph.addPass(pass);
        }

        void compile();
        DrawCommandList execute(ThreadPool& pool, const RenderGraphExecuteContext& context);
        void reset();

        [[nodiscard]] RenderGraph& graph() { return m_graph; }
        [[nodiscard]] const RenderGraph& graph() const { return m_graph; }
        [[nodiscard]] RenderGraphBlackboard& blackboard() { return m_blackboard; }
        [[nodiscard]] const RenderGraphBlackboard& blackboard() const { return m_blackboard; }

        UInt32 registerTexture(const RenderGraphTextureDesc& desc, const String& name);
        UInt32 registerBuffer(const RenderGraphBufferDesc& desc, const String& name);
        UInt32 registerImportedTexture(const GfxTextureHandle& texture, const String& name);
        UInt32 registerImportedBuffer(const GfxBufferHandle& buffer, const String& name);
        UInt32 registerBackBuffer(const String& name);

        friend class RenderGraphPassBuilder;
    };

} // dodoe