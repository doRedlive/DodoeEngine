// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/trace_visitor.h"

namespace dodoe {

    class CycleDetector {
        UInt64 m_frame_count{0};
        UInt64 m_interval{60};
        Size_t m_threshold{1024};

        CycleDetector() = default;

    public:
        static CycleDetector& instance();

        void tick();
        void setInterval(UInt64 frames) { m_interval = frames; }
        void setThreshold(Size_t count) { m_threshold = count; }
    };

} // namespace dodoe
