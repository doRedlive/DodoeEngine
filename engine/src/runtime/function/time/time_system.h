//
// Created by GreenMuffin on 2025/10/27.
//

#ifndef DODOE_TIME_H
#define DODOE_TIME_H

#include "dopch.h"

namespace dodoe {
    class TimeSystem {

    public:
        TimeSystem() = default;
        ~TimeSystem() = default;

        [[nodiscard]] float get_delta_time() const;
        [[nodiscard]] float get_time_scale() const;
        [[nodiscard]] int   get_target_fps() const;
        [[nodiscard]] int   get_fps() const;
        [[nodiscard]] float get_unscaled_delta_time() const;

        void set_time_scale(float time_scale);
        void set_target_fps(int target_fps);

        void calculate_time();

    private:
        float delta_time_ {0.0f};
        float time_scale_ {1.0f};

        std::chrono::steady_clock::time_point last_time_point_ { std::chrono::steady_clock::now() };

        int target_fps_ {-1};
    };
}
#endif //DODOE_TIME_H