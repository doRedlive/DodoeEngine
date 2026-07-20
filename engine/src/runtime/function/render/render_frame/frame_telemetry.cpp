// do@Redlive

#include "frame_telemetry.h"

#include <sstream>

namespace dodoe {

    String FrameTelemetry::toJSON() const {
        std::ostringstream ss;
        ss << "{";
        ss << "\"frame\":" << frame_number << ",";
        ss << "\"delta_ms\":" << delta_time_ms << ",";
        ss << "\"game_ms\":" << game_thread_ms << ",";
        ss << "\"render_ms\":" << render_thread_ms << ",";
        ss << "\"draw_ms\":" << draw_thread_ms << ",";
        ss << "\"gpu_ms\":" << gpu_frame_ms << ",";
        ss << "\"upload_bytes\":" << upload_bytes << ",";
        ss << "\"upload_stall\":" << upload_stall_count << ",";
        ss << "\"upload_overflow\":" << upload_overflow_count << ",";
        ss << "\"arena_mb\":" << frame_arena_used_mb << ",";
        ss << "\"arena_peak_mb\":" << frame_arena_peak_mb << ",";
        ss << "\"draw_calls\":" << draw_call_count << ",";
        ss << "\"dispatches\":" << dispatch_count << ",";
        ss << "\"barriers\":" << barrier_count << ",";
        ss << "\"pending_deletions\":" << pending_deletion_count;
        ss << "}";
        return String(ss.str().c_str());
    }

    void FrameTelemetryCollector::record(const FrameTelemetry& telemetry) {
        m_history[m_write_index] = telemetry;
        m_write_index = (m_write_index + 1) % kHistorySize;
        if (m_count < kHistorySize) {
            m_count++;
        }
    }

    const FrameTelemetry& FrameTelemetryCollector::current() const {
        if (m_count == 0) {
            static const FrameTelemetry k_empty{};
            return k_empty;
        }
        Size_t idx = (m_write_index == 0) ? kHistorySize - 1 : m_write_index - 1;
        return m_history[idx];
    }

    const FrameTelemetry& FrameTelemetryCollector::previous(UInt32 frames_ago) const {
        if (frames_ago >= m_count) {
            static const FrameTelemetry k_empty{};
            return k_empty;
        }
        Size_t idx = (m_write_index + kHistorySize - 1 - frames_ago) % kHistorySize;
        return m_history[idx];
    }

} // namespace dodoe
