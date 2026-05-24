//
// Created by Redlive on 2026/3/23.
//

#include "animation_library.h"

#include "runtime/core/utils/common.h"

namespace dodoe {

    bool AnimationLibrary::initialize(const AnimationLibraryCreateInfo& create_info) {
        (void)create_info;
        anim_manager_ = AnimationManager::Create({});
        return anim_manager_ != nullptr;
    }

    void AnimationLibrary::shutdown() {
        anim_clip2d_umap_.clear();
        AnimationManager::Destroy(anim_manager_);
    }

    AnimClip2dRes AnimationLibrary::create_clip(const std::string& name, const std::vector<identifier>& texture_ids, bool loop, float frame_ms) {
        AnimClip2d clip = anim_manager_->create_anim_clip2d(texture_ids);
        clip.loop = loop;
        if (frame_ms > 0.0f) {
            for (auto& f : clip.frames) {
                f.duration = frame_ms;
            }
        }

        const identifier id = static_cast<identifier>(string2hash(name));
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
        const auto id = string2hash(name);
        return destroy_clip(id);
    }

    bool AnimationLibrary::has_clip(const identifier id) const {
        return anim_clip2d_umap_.find(id) != anim_clip2d_umap_.end();
    }

    bool AnimationLibrary::has_clip(const std::string& name) const {
        return has_clip(static_cast<identifier>(string2hash(name)));
    }

    AnimClip2dRes AnimationLibrary::get_clip(const identifier id) {
        auto it = anim_clip2d_umap_.find(id);
        if (it == anim_clip2d_umap_.end()) {
            return {};
        }
        return it->second;
    }

    AnimClip2dRes AnimationLibrary::get_clip(const std::string& name) {
        return get_clip(static_cast<identifier>(string2hash(name)));
    }

} // dodoe

