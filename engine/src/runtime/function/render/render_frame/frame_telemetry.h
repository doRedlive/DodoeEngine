// do@Redlive

#pragma once

#include "dopch.h"

#include <atomic>

namespace dodoe {

    struct FrameTelemetry {
        UInt64 frame_number{0};
        Float delta_time_ms{0.0f};

        Float game_thread_ms{0.0f};
        Float render_thread_ms{0.0f};

        Float gpu_frame_ms{0.0f};

        UInt64 upload_bytes{0};
        UInt32 upload_stall_count{0};
        UInt32 upload_overflow_count{0};

        Float frame_arena_used_mb{0.0f};
        Float frame_arena_peak_mb{0.0f};

        UInt32 draw_call_count{0};
        UInt32 indirect_draw_call_count{0};
        UInt32 dispatch_count{0};
        UInt32 barrier_count{0};

        UInt64 drawn_instance_count{0};

        UInt32 pending_deletion_count{0};

        String toJSON() const;
    };

#ifdef DODOE_PERF_ENABLED
    class RenderFrameCounters {
        std::atomic<UInt64> m_draw_calls{0};
        std::atomic<UInt64> m_indirect_draw_calls{0};
        std::atomic<UInt64> m_dispatches{0};
        std::atomic<UInt64> m_barriers{0};
        std::atomic<UInt64> m_drawn_instances{0};
    public:
        static RenderFrameCounters& Self();

        void reset();
        void addDrawCall(UInt32 instance_count);
        void addIndirectDrawCall(UInt32 draw_count);
        void addDispatch();
        void addBarrier();

        [[nodiscard]] UInt64 getDrawCalls() const { return m_draw_calls.load(std::memory_order_relaxed); }
        [[nodiscard]] UInt64 getIndirectDrawCalls() const { return m_indirect_draw_calls.load(std::memory_order_relaxed); }
        [[nodiscard]] UInt64 getDispatches() const { return m_dispatches.load(std::memory_order_relaxed); }
        [[nodiscard]] UInt64 getBarriers() const { return m_barriers.load(std::memory_order_relaxed); }
        [[nodiscard]] UInt64 getDrawnInstances() const { return m_drawn_instances.load(std::memory_order_relaxed); }
    };
#else
    class RenderFrameCounters {
    public:
        static RenderFrameCounters& Self();

        void reset() {}
        void addDrawCall(UInt32) {}
        void addIndirectDrawCall(UInt32) {}
        void addDispatch() {}
        void addBarrier() {}
    };
#endif//DODOE_PERF_ENABLED

    class FrameTelemetryCollector {
        static constexpr Size_t kHistorySize = 256;
        FrameTelemetry m_history[kHistorySize]{};
        Size_t m_write_index{0};
        Size_t m_count{0};

    public:
        void record(const FrameTelemetry& telemetry);

        [[nodiscard]] const FrameTelemetry& current() const;
        [[nodiscard]] const FrameTelemetry& previous(UInt32 frames_ago = 1) const;

        [[nodiscard]] Size_t getCount() const { return m_count; }
        [[nodiscard]] Size_t getCapacity() const { return kHistorySize; }
    };

} // namespace dodoe
