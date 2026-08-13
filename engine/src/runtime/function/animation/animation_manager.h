//
// Created by Redlive on 2026/3/23.
//

#pragma once

#include "dopch.h"

#include "animation.h"

namespace dodoe {

    struct AnimationManagerCreateInfo {

    };

    class AnimationManager : public Managed<AnimationManager, AnimationManagerCreateInfo> {
        friend class Managed<AnimationManager, AnimationManagerCreateInfo>;
    public:

    private:
        bool initialize(const AnimationManagerCreateInfo& create_info);
        void shutdown();

    };

} // dodoe
