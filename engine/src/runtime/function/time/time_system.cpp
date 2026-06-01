//
// Created by GreenMuffin on 2025/10/27.
//

#include "runtime/function/time/time_system.h"

namespace dodoe {

    float TimeSystem::getDeltaTime() {
        calculate_time();
        return delta_time_ * time_scale_;
    }

    float TimeSystem::current_time() const {
        return cur_time_;
    }

    float TimeSystem::get_time_scale() const {
        return time_scale_;
    }

    int TimeSystem::get_fps() const {
        return static_cast<int>(1.0f / delta_time_); // dt = 1s / fps;
    }

    int TimeSystem::get_target_fps() const {
        return target_fps_;
    }

    float TimeSystem::get_unscaled_delta_time() const {
        return delta_time_;
    }

    void TimeSystem::set_time_scale(float time_scale) {
        if (time_scale < 0.0f) {
            time_scale = 1.0f;
            DO_WARN("TimeSystem scale cannot be negative. Resetting to 1.0f.");
        }
        time_scale_ = time_scale;
    }

    void TimeSystem::set_target_fps(const int fps) {
        target_fps_ = fps;
    }

    void TimeSystem::calculate_time() {
        using namespace std::chrono;
        if (target_fps_ > 0) {
            const auto time_to_wait_point = duration<float>(1.0f / static_cast<float>(target_fps_)) + last_time_point_;
            std::this_thread::sleep_until(time_to_wait_point);
        }
        const auto frame_start_time_point = steady_clock::now();
        delta_time_ = duration_cast<duration<float>>(frame_start_time_point - last_time_point_).count();
        last_time_point_ = frame_start_time_point;

        cur_time_ += delta_time_;
    }
}
