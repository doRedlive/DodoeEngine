//
// Created by Redlive on 2026/3/20.
//

#include "animation_manager.h"

#include "core/utils/common.h"

namespace dodoe {

    Scope<AnimationManager> AnimationManager::create(const AnimationManagerCreateInfo& create_info) {
        auto context = create_scope<AnimationManager>();
        context->initialize(create_info);
        return context;
    }

    void AnimationManager::destroy(Scope<AnimationManager>& animation_mananger) {
        if (!animation_mananger) return;
        animation_mananger->shutdown();
        animation_mananger.reset();
    }

    AnimClip2d AnimationManager::create_anim_clip2d(const std::vector<identifier>& texture_ids) {
        std::vector<AnimFrame2d> frames;
        frames.reserve(texture_ids.size());
        for (auto& texture_id : texture_ids) {
            frames.emplace_back(texture_id);
        }

        return AnimClip2d{frames};
    }

    void AnimationManager::initialize(const AnimationManagerCreateInfo& create_info) {

    }

    void AnimationManager::shutdown() {

    }

} // dodoe