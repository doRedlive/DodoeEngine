// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/texture/texture.h"

REFLECTION_TYPE(SkyLightComponent)

namespace dodoe {

    STRUCT(SkyLightComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(SkyLightComponent)

        META(Enable)
        PPtr<TextureCubemap> cubemap{};
        META(Enable)
        Float intensity{1.0f};

        Bool enabled{true};
        Bool dirty{true};
    };

} // dodoe
