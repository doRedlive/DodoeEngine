// do@Redlive

#include "render_graph_debug.h"

#include <chrono>
#include <mutex>

namespace dodoe {

    namespace {
        std::mutex s_mutex;
        std::shared_ptr<const RenderGraphDebugSnapshot> s_snapshot;
        Bool s_auto_refresh{true};
        UInt32 s_refresh_interval_ms{500};
        UInt64 s_last_publish_ms{0};
        Bool s_force_publish{true};

        UInt64 now_ms() {
            return static_cast<UInt64>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }
    }

    void RenderGraphDebug::publish(const RenderGraph& graph) {
        const UInt64 now = now_ms();
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (!s_force_publish) {
                if (!s_auto_refresh) return;
                if (now - s_last_publish_ms < s_refresh_interval_ms) return;
            }
            s_force_publish = false;
            s_last_publish_ms = now;
        }

        auto snap = capture(graph);

        std::lock_guard<std::mutex> lock(s_mutex);
        static UInt64 s_sequence{0};
        snap->sequence = ++s_sequence;
        s_snapshot = std::move(snap);
    }

    void RenderGraphDebug::requestRefresh() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_force_publish = true;
    }

    void RenderGraphDebug::setAutoRefresh(const Bool enabled) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_auto_refresh = enabled;
    }

    void RenderGraphDebug::setRefreshIntervalMs(const UInt32 milliseconds) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_refresh_interval_ms = milliseconds;
    }

    std::shared_ptr<const RenderGraphDebugSnapshot> RenderGraphDebug::snapshot() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_snapshot;
    }

    std::shared_ptr<RenderGraphDebugSnapshot> RenderGraphDebug::capture(const RenderGraph& graph) {
        auto snap = std::make_shared<RenderGraphDebugSnapshot>();

        const auto& passes = graph.getPasses();
        const auto& resources = graph.getResources();
        const auto& levels = graph.getLevels();
        const auto& culled = graph.getCulledPasses();

        snap->pass_count = static_cast<UInt32>(passes.size());
        snap->resource_count = static_cast<UInt32>(resources.size());
        snap->level_count = static_cast<UInt32>(levels.size());
        snap->subgraph_names = graph.getSubgraphNames();

        DynamicArray<UInt32> pass_levels(passes.size(), 0);
        for (Size_t level = 0; level < levels.size(); level++) {
            DynamicArray<UInt32> snap_level{};
            snap_level.reserve(levels[level].size());
            for (const auto pass_index : levels[level]) {
                if (pass_index >= passes.size()) continue;
                pass_levels[pass_index] = static_cast<UInt32>(level);
                snap_level.push_back(static_cast<UInt32>(pass_index));
            }
            snap->levels.push_back(std::move(snap_level));
        }

        snap->passes.resize(passes.size());
        for (Size_t i = 0; i < passes.size(); i++) {
            auto& out_pass = snap->passes[i];
            const auto& pass = passes[i];

            out_pass.name = pass->getName();
            out_pass.flags = pass->getFlags();
            out_pass.culled = i < culled.size() ? culled[i] : false;
            out_pass.subgraph_index = pass->getSubgraphIndex();
            out_pass.barrier_count = static_cast<UInt32>(pass->getPreBarriers().size());
            out_pass.level = pass_levels[i];

            const auto& accesses = pass->getAccesses();
            out_pass.accesses.resize(accesses.size());
            for (Size_t j = 0; j < accesses.size(); j++) {
                out_pass.accesses[j].resource_index = accesses[j].resource_index;
                out_pass.accesses[j].access_type = accesses[j].access_type;
                out_pass.accesses[j].stage = accesses[j].stage;
            }

            if (out_pass.culled) snap->culled_count++;
        }

        snap->resources.resize(resources.size());
        for (Size_t i = 0; i < resources.size(); i++) {
            auto& out_resource = snap->resources[i];
            const auto& resource = resources[i];

            out_resource.name = resource.name;
            out_resource.type = resource.type;
            out_resource.imported = resource.isImported();
            out_resource.exported = resource.is_exported;
            out_resource.first_pass_index = resource.first_pass_index;
            out_resource.last_pass_index = resource.last_pass_index;
            out_resource.writer_passes = resource.writer_passes;
            out_resource.reader_passes = resource.reader_passes;
        }

        return snap;
    }

} // dodoe
