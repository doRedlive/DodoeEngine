// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph_pass.h"
#include "render_graph_resource.h"
#include "runtime/function/graphics/draw_command_list.h"

#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    class RenderGraph {
        DynamicArray<Ref<RenderGraphPass>> m_passes{};
        DynamicArray<RenderGraphResourceRecord> m_resources{};
        DynamicArray<DynamicArray<Size_t>> m_levels{};
        DynamicArray<Bool> m_culled_passes{};
        Bool m_compiled{false};

    public:
        RenderGraph() = default;

        void addPass(const Ref<RenderGraphPass>& pass);
        UInt32 addTextureResource(const RenderGraphTextureDesc& desc, const String& name);
        UInt32 addBufferResource(const RenderGraphBufferDesc& desc, const String& name);
        void compile();
        DrawCommandList execute(ThreadPool& pool, const RenderGraphExecuteContext& context);
        void reset();

        [[nodiscard]] Bool isCompiled() const { return m_compiled; }
        [[nodiscard]] const DynamicArray<Ref<RenderGraphPass>>& getPasses() const { return m_passes; }
        [[nodiscard]] DynamicArray<RenderGraphResourceRecord>& getResources() { return m_resources; }
        [[nodiscard]] const DynamicArray<RenderGraphResourceRecord>& getResources() const { return m_resources; }
    };

} // dodoe
