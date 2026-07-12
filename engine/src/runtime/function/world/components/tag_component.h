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
        std::string tag{"default"};

        identifier id{string2hash("default")};
        bool dirty{false};

        void setTag(const std::string& v) { tag = v; id = string2hash(v); dirty = true; }
        [[nodiscard]] const std::string& getTag() const { return tag; }
    };

} // dodoe
