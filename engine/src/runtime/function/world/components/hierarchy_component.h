// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/world/entity.h"

REFLECTION_TYPE(HierarchyComponent)

namespace dodoe {

    STRUCT(HierarchyComponent, WhiteListFields, ScriptBind) {
        REFLECTION_BODY(HierarchyComponent)

        META(Enable)
        UUID parent_uuid{};
        META(Enable)
        int child_count{0};

        Entity parent;
        std::vector<Entity> children;

        bool dirty{true};
    };

} // dodoe
