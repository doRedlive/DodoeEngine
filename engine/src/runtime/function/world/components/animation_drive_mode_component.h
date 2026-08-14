// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(AnimationDriveModeComponent)

namespace dodoe {
    STRUCT(AnimationDriveModeComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(AnimationDriveModeComponent)

        enum class DriveMode {
            Animated = 0,
            Kinematic = 1,
            Ragdoll = 2,
            Partial = 3
        };

        META(Enable)
        DriveMode mode{ DriveMode::Animated };
        META(Enable)
        bool enabled{ true };
    };

} // dodoe
