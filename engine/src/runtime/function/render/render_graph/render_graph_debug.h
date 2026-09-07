// do@Redlive

#pragma once

#include "dopch.h"

#include "render_graph.h"

namespace dodoe {

    struct RenderGraphDebugAccess {
        UInt32 resource_index{kInvalidRenderGraphHandle};
        RenderGraphAccessType access_type{RenderGraphAccessType::Read};
        RenderGraphPipelineStage stage{RenderGraphPipelineStage::PixelShader};
    };

    struct RenderGraphDebugPass {
        String name{};
        RenderGraphPassFlags flags{RenderGraphPassFlags::None};
        Bool culled{false};
        UInt32 subgraph_index{~0u};
        UInt32 barrier_count{0};
        UInt32 level{0};
        DynamicArray<RenderGraphDebugAccess> accesses{};
    };

    struct RenderGraphDebugResource {
        String name{};
        RenderGraphResourceType type{RenderGraphResourceType::Texture};
        Bool imported{false};
        Bool exported{false};
        Int32 first_pass_index{-1};
        Int32 last_pass_index{-1};
        DynamicArray<UInt32> writer_passes{};
        DynamicArray<UInt32> reader_passes{};
    };

    struct RenderGraphDebugSnapshot {
        UInt64 sequence{0};
        UInt32 pass_count{0};
        UInt32 culled_count{0};
        UInt32 resource_count{0};
        UInt32 level_count{0};
        DynamicArray<RenderGraphDebugPass> passes{};
        DynamicArray<RenderGraphDebugResource> resources{};
        DynamicArray<DynamicArray<UInt32>> levels{};
        DynamicArray<String> subgraph_names{};
    };

    class RenderGraphDebug {
    public:
        static void publish(const RenderGraph& graph);
        static void requestRefresh();
        static void setAutoRefresh(const Bool enabled);
        static void setRefreshIntervalMs(const UInt32 milliseconds);

        [[nodiscard]] static std::shared_ptr<const RenderGraphDebugSnapshot> snapshot();

    private:
        [[nodiscard]] static std::shared_ptr<RenderGraphDebugSnapshot> capture(const RenderGraph& graph);
    };

} // dodoe
