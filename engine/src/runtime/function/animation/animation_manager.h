//
// Created by Redlive on 2026/3/23.
//

#pragma once

#include "dopch.h"

#include "anim_clip_2d.h"

namespace dodoe {

    struct AnimationManagerCreateInfo {

    };

    class AnimationManager : public Managed<AnimationManager, AnimationManagerCreateInfo> {
        friend class Managed<AnimationManager, AnimationManagerCreateInfo>;
    public:

        // MARK: TODO: using sprite instead of texture
        AnimClip2D CreateAnimClip2D(const DynamicArray<InstanceID>& texture_ids);

    private:
        bool initialize(const AnimationManagerCreateInfo& create_info);
        void shutdown();

    };

} // dodoe
