//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_ANIMATION_H
#define DODOE_ANIMATION_H

#include "dopch.h"

namespace dodoe {

    // MARK: TODO: using the sprite instead of texture
    struct AnimFrame2d {
        identifier texture_id{0};
        float duration{100.0f}; // ------- ms

        AnimFrame2d() = default;
        AnimFrame2d(identifier in_texture_id) : texture_id(in_texture_id) { }
    };

    struct AnimClip2d {
        std::vector<AnimFrame2d> frames{};
        bool loop{false};

        AnimClip2d() = default;
        AnimClip2d(const std::vector<AnimFrame2d>& in_frames) :  frames(in_frames) { }
    };

    class Animation {

    };

} // dodoe

#endif//DODOE_ANIMATION_H