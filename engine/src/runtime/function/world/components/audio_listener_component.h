// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(AudioListenerComponent)

namespace dodoe {

    STRUCT(AudioListenerComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(AudioListenerComponent)

        AudioListenerComponent() = default;
    };

} // dodoe
