// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/memory/managed.h"

namespace dodoe {

    struct TimeSystemCreateInfo {};

    class TimeSystem : public Managed<TimeSystem, TimeSystemCreateInfo> {
        friend class Managed<TimeSystem, TimeSystemCreateInfo>;

    public:
        TimeSystem() = default;
        ~TimeSystem() = default;

        [[nodiscard]] float getDeltaTime();
        [[nodiscard]] float getCurrentTime() const;
        [[nodiscard]] float getTimeScale() const;
        [[nodiscard]] int   get_target_fps() const;
        [[nodiscard]] int   getFps() const;
        [[nodiscard]] float getUnscaledDeltaTime() const;

        void setTimeScale(float time_scale);
        void setTargetFps(int target_fps);

    private:
        [[nodiscard]] bool initialize(const TimeSystemCreateInfo&) { return true; }
        void shutdown() {}

        float delta_time_{0.0f};
        float time_scale_{1.0f};
        float cur_time_{0.0f};

        std::chrono::steady_clock::time_point last_time_point_{std::chrono::steady_clock::now()};

        int target_fps_{-1};
        void calculateTime();
    };

} // dodoe
