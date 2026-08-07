// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/common.h"

REFLECTION_TYPE(TagComponent)

namespace dodoe {

    STRUCT(TagComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(TagComponent)

        META(Enable)
        String tag{"default"};

        identifier id{string2hash("default")};
        bool dirty{true};

        void setTag(const String& v) { tag = v; id = string2hash(v); dirty = true; }
        [[nodiscard]] const String& getTag() const { return tag; }
    };

} // dodoe
