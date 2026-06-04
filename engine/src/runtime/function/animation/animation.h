//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_ANIMATION_H
#define DODOE_ANIMATION_H

#include "dopch.h"

namespace dodoe {

    struct AnimFrame2D {
        InstanceID texture_id{0};
        Float duration{100.0f};

        AnimFrame2D() = default;
        explicit AnimFrame2D(const InstanceID in_texture_id) : texture_id(in_texture_id) {}
    };

    struct AnimClip2D {
        DynamicArray<AnimFrame2D> frames{};
        Bool loop{false};

        AnimClip2D() = default;
        explicit AnimClip2D(const DynamicArray<AnimFrame2D>& in_frames) : frames(in_frames) {}
    };

    struct AnimClip2DRes {
        Ref<AnimClip2D> clip;
        String name;
        InstanceID id;
    };

    class Animation {

    };

} // dodoe

#endif//DODOE_ANIMATION_H
