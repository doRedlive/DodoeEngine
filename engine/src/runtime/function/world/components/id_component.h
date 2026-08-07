// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"

REFLECTION_TYPE(IDComponent)

namespace dodoe {

    STRUCT(IDComponent, WhiteListFields) {
        REFLECTION_BODY(IDComponent)

        META(Enable)
        UUID id{};
        META(Enable)
        String name{};

        IDComponent() = default;
        IDComponent(const UUID& in_id, String in_name) : id(in_id), name(std::move(in_name)) {}
        IDComponent(const IDComponent&) = default;

        bool dirty{true};

        void setName(const String& in_name) { name = in_name; dirty = true; }
        [[nodiscard]] const String& getName() const { return name; }
    };

} // dodoe