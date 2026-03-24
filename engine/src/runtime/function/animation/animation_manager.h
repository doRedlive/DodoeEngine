//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_ANIMATION_MANAGER
#define DODOE_ANIMATION_MANAGER

#include "dopch.h"

#include "animation.h"

namespace dodoe {

    struct AnimationManagerCreateInfo {

    };

    class AnimationManager {
    public:
        static Scope<AnimationManager> create(const AnimationManagerCreateInfo& create_info);
        static void destroy(Scope<AnimationManager>& animation_manager);

        // MARK: TODO: using sprite instead of texture
        AnimClip2d create_anim_clip2d(const std::vector<identifier>& texture_ids);

    private:
        void initialize(const AnimationManagerCreateInfo& create_info);
        void shutdown();

    };

} // dodoe

#endif//DODOE_ANIMATION_MANAGER