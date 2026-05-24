//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_ANIMATION_LIBRARY_H
#define DODOE_ANIMATION_LIBRARY_H

#include "dopch.h"

#include "runtime/resource/resource_type.h"
#include "runtime/function/animation/animation_manager.h"

namespace dodoe {

    struct AnimationLibraryCreateInfo {
    };

    class AnimationLibrary : public Managed<AnimationLibrary, AnimationLibraryCreateInfo> {
        friend class Managed<AnimationLibrary, AnimationLibraryCreateInfo>;
    public:

        AnimClip2dRes create_clip(const std::string& name, const std::vector<identifier>& texture_ids, bool loop = false, float frame_ms = 100.0f);
        bool destroy_clip(identifier id);
        bool destroy_clip(const std::string& name);
        bool has_clip(identifier id) const;
        bool has_clip(const std::string& name) const;
        AnimClip2dRes get_clip(identifier id);
        AnimClip2dRes get_clip(const std::string& name);

    private:
        bool initialize(const AnimationLibraryCreateInfo& create_info);
        void shutdown();

        Scope<AnimationManager> anim_manager_{nullptr};
        std::unordered_map<identifier, AnimClip2dRes> anim_clip2d_umap_{};
    };

} // dodoe

#endif//DODOE_ANIMATION_LIBRARY_H
