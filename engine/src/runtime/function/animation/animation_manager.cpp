//
// Created by Redlive on 2026/3/20.
//

#include "animation_manager.h"

namespace dodoe {

    AnimClip2D AnimationManager::CreateAnimClip2D(const DynamicArray<InstanceID>& texture_ids) {
        DynamicArray<AnimFrame2D> frames;
        frames.reserve(texture_ids.size());
        for (const auto& texture_id : texture_ids) {
            frames.emplace_back(texture_id);
        }

        return AnimClip2D{frames};
    }

    Bool AnimationManager::initialize(const AnimationManagerCreateInfo& create_info) {
        (void)create_info;
        return true;
    }

    void AnimationManager::shutdown() {

    }

} // dodoe
