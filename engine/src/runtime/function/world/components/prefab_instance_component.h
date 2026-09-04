// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/world/prefab.h"

REFLECTION_TYPE(PrefabInstanceComponent)

namespace dodoe {

    STRUCT(PrefabInstanceComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(PrefabInstanceComponent)

        META(Enable)
        PPtr<Prefab> prefab{};
        META(Enable)
        Vector3f position{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f rotation{ 0.0f, 0.0f, 0.0f };
        META(Enable)
        Vector3f scale{ 1.0f, 1.0f, 1.0f };
    };

} // dodoe
