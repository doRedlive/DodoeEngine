//
// Created by Redlive on 2026/3/23.
//

#include "animation_library.h"

#include "runtime/core/utils/common.h"

namespace dodoe {

    Scope<AnimationLibrary> AnimationLibrary::create(AnimationLibraryCreateInfo create_info) {
        auto context = create_scope<AnimationLibrary>();
        context->initialize(create_info);
        return context;
    }

    void AnimationLibrary::destroy(Scope<AnimationLibrary>& animation_library) {
        if (!animation_library) {
            return;
        }
        animation_library->shutdown();
        animation_library.reset();
    }

    void AnimationLibrary::initialize(AnimationLibraryCreateInfo create_info) {
        (void)create_info;
        anim_manager_ = AnimationManager::create({});
    }

    void AnimationLibrary::shutdown() {
        anim_clip2d_umap_.clear();
        AnimationManager::destroy(anim_manager_);
    }

    AnimClip2dRes AnimationLibrary::create_clip(const std::string& name, const std::vector<identifier>& texture_ids, bool loop, float frame_ms) {
        AnimClip2d clip = anim_manager_->create_anim_clip2d(texture_ids);
        clip.loop = loop;
        if (frame_ms > 0.0f) {
            for (auto& f : clip.frames) {
                f.duration = frame_ms;
            }
        }

        const identifier id = static_cast<identifier>(String2Hash(name));
        AnimClip2dRes res{create_ref<AnimClip2d>(clip), name, id};
        anim_clip2d_umap_.emplace(id, res);
        return res;
    }

    bool AnimationLibrary::destroy_clip(const identifier id) {
        auto it = anim_clip2d_umap_.find(id);
        if (it == anim_clip2d_umap_.end()) {
            return false;
        }
        anim_clip2d_umap_.erase(it);
        return true;
    }

    bool AnimationLibrary::destroy_clip(const std::string& name) {
        const auto id = String2Hash(name);
        return destroy_clip(id);
    }

    bool AnimationLibrary::has_clip(const identifier id) const {
        return anim_clip2d_umap_.find(id) != anim_clip2d_umap_.end();
    }

    bool AnimationLibrary::has_clip(const std::string& name) const {
        return has_clip(static_cast<identifier>(String2Hash(name)));
    }

    AnimClip2dRes AnimationLibrary::get_clip(const identifier id) {
        auto it = anim_clip2d_umap_.find(id);
        if (it == anim_clip2d_umap_.end()) {
            return {};
        }
        return it->second;
    }

    AnimClip2dRes AnimationLibrary::get_clip(const std::string& name) {
        return get_clip(static_cast<identifier>(String2Hash(name)));
    }

} // dodoe

