// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/resource/resource_type.h"

namespace dodoe {

    struct Animation2dComponent {
        std::unordered_map<identifier, Ref<AnimClip2d>> anim_clip_umap{};
        identifier cur_anim_id{0};
        size_t cur_frame_id{0};
        float cur_time_duration{0.0f};
        float speed{1.0f};

        void addClip(AnimClip2dRes res) {
            anim_clip_umap.emplace(res.id, res.clip);
            cur_anim_id = res.id;
        }
    };

} // dodoe