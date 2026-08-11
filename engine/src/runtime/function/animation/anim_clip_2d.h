// do@Redlive

#pragma once

#include "dopch.h"

#include "animation.h"

namespace dodoe {

    struct AnimFrame2D {
        InstanceID texture_id{0};
        Float duration{100.0f};

        AnimFrame2D() = default;
        explicit AnimFrame2D(const InstanceID in_texture_id) : texture_id(in_texture_id) {}
    };

    struct AnimClip2D {
        DynamicArray<AnimFrame2D> frames{};
        DynamicArray<AnimClipEvent> events{};
        Bool loop{false};

        AnimClip2D() = default;
        explicit AnimClip2D(const DynamicArray<AnimFrame2D>& in_frames) : frames(in_frames) {}

        [[nodiscard]] Float totalDurationMs() const {
            Float total = 0.0f;
            for (const auto& frame : frames) {
                total += frame.duration;
            }
            return total;
        }
    };

    struct AnimClip2DRes {
        Ref<AnimClip2D> clip;
        String name;
        InstanceID id;
    };

} // dodoe
