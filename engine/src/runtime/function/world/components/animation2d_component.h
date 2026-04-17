// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/resource/resource_type.h"

REFLECTION_TYPE(Animation2dComponent)

namespace dodoe {

    STRUCT(Animation2dComponent, WhiteListFields) {
        REFLECTION_BODY(Animation2dComponent)

        META(Enable)
        std::unordered_map<identifier, Ref<AnimClip2d>> anim_clip_umap{};
        META(Enable)
        identifier cur_anim_id{0};
        META(Enable)
        size_t cur_frame_id{0};
        META(Enable)
        float cur_time_duration{0.0f};
        META(Enable)
        float speed{1.0f};

        void addClip(AnimClip2dRes res) {
            anim_clip_umap.emplace(res.id, res.clip);
            cur_anim_id = res.id;
        }
    };

} // dodoe