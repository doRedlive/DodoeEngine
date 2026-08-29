// do@Redlive

#pragma once

#include "dopch.h"

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
        UInt32 dispatch_count{0};
        UInt32 barrier_count{0};

        UInt32 pending_deletion_count{0};

        String toJSON() const;
    };

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
