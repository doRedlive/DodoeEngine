// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_pass.h"
#include "render_graph_resource.h"
#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    class RenderGraph {
        DynamicArray<Ref<RenderGraphPass>> m_passes{};
        DynamicArray<RenderGraphResourceRecord> m_resources{};
        DynamicArray<DynamicArray<Size_t>> m_levels{};
        DynamicArray<Bool> m_culled_passes{};
        DynamicArray<String> m_subgraph_names{};
        Bool m_compiled{false};

    public:
        RenderGraph() = default;

        String dumpToJSON() const;
        String dumpToDOT() const;

        void addPass(const Ref<RenderGraphPass>& pass);
        UInt32 addSubgraph(const String& name);
        UInt32 addTextureResource(const RenderGraphTextureDesc& desc, const String& name);
        UInt32 addBufferResource(const RenderGraphBufferDesc& desc, const String& name);
        UInt32 addImportedTextureResource(const GfxTextureHandle& texture, const String& name);
        UInt32 addImportedBufferResource(const GfxBufferHandle& buffer, const String& name);
        UInt32 addBackBufferResource(const String& name);
        void compile();
        void execute(ThreadPool& pool, const RenderGraphExecuteContext& context, DrawCommandList& out_commands);
        void reset();

        [[nodiscard]] Bool isCompiled() const { return m_compiled; }
        [[nodiscard]] const DynamicArray<Ref<RenderGraphPass>>& getPasses() const { return m_passes; }
        [[nodiscard]] DynamicArray<RenderGraphResourceRecord>& getResources() { return m_resources; }
        [[nodiscard]] const DynamicArray<RenderGraphResourceRecord>& getResources() const { return m_resources; }
        [[nodiscard]] const DynamicArray<String>& getSubgraphNames() const { return m_subgraph_names; }

    private:
        void resetResourceTracking();
        void buildDependencyGraph(DynamicArray<DynamicArray<Size_t>>& edges, DynamicArray<Int32>& indegree);
        void topologicalSort(const DynamicArray<DynamicArray<Size_t>>& edges, const DynamicArray<Int32>& indegree);
        void validateAccesses();
        void deriveBarriers();
        void cullUnreachablePasses();
    };

} // dodoe
