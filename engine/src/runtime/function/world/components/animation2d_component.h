// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/animation/animation.h"

REFLECTION_TYPE(Animation2dComponent)

namespace dodoe {

    STRUCT(Animation2dComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(Animation2dComponent)

        META(Enable)
        std::unordered_map<InstanceID, Ref<AnimClip2D>> anim_clip_umap{};
        META(Enable)
        InstanceID cur_anim_id{0};
        META(Enable)
        size_t cur_frame_id{0};
        META(Enable)
        float cur_time_duration{0.0f};
        META(Enable)
        float speed{1.0f};

        void addClip(AnimClip2DRes res) {
            anim_clip_umap.emplace(res.id, res.clip);
            cur_anim_id = res.id;
        }
    };

} // dodoe