// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"

REFLECTION_TYPE(ActiveComponent)

namespace dodoe {

    STRUCT(ActiveComponent, WhiteListFields) {
        REFLECTION_BODY(ActiveComponent)

        META(Enable)
        bool active_self{true};

        ActiveComponent() = default;
        explicit ActiveComponent(bool in_active) : active_self(in_active) {}
        ActiveComponent(const ActiveComponent&) = default;

        bool dirty{true};

        void setActive(bool in_active) { active_self = in_active; dirty = true; }
        [[nodiscard]] bool isActiveSelf() const { return active_self; }
    };

} // dodoe
