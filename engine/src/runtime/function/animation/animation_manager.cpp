//
// Created by Redlive on 2026/3/20.
//

#include "animation_manager.h"

#include "runtime/core/utils/common.h"

namespace dodoe {

    AnimClip2d AnimationManager::create_anim_clip2d(const std::vector<identifier>& texture_ids) {
        std::vector<AnimFrame2d> frames;
        frames.reserve(texture_ids.size());
        for (auto& texture_id : texture_ids) {
            frames.emplace_back(texture_id);
        }

        return AnimClip2d{frames};
    }

    bool AnimationManager::initialize(const AnimationManagerCreateInfo& create_info) {
        (void)create_info;
        return true;
    }

    void AnimationManager::shutdown() {

    }

} // dodoe
