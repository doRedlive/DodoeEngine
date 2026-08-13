// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/animation/animation.h"
#include "runtime/function/animation/animator_controller.h"

REFLECTION_TYPE(AnimatorComponent)

namespace dodoe {

    STRUCT(AnimatorComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(AnimatorComponent)

        META(Enable)
        Ref<AnimatorController> controller{};
        META(Enable)
        Float speed{1.0f};
        META(Enable)
        Bool play_on_awake{true};

        String play_request{};
        Bool stop_requested{false};
        Bool resume_requested{false};

        Size_t cur_state{0};
        Float state_time{0.0f};
        Float prev_state_time{0.0f};
        Size_t cur_frame_id{0};
        Size_t applied_frame_id{static_cast<Size_t>(-1)};
        Bool playing{false};
        UnorderedMap<String, Float> parameters{};
        DynamicArray<String> pending_events{};
    };

} // dodoe
