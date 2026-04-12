// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"

namespace dodoe {

    struct IDComponent {
        Uuid id{};
        std::string name{};

        IDComponent() = default;
        IDComponent(const Uuid& in_id, std::string in_name) : id(in_id), name(std::move(in_name)) {}
        IDComponent(const IDComponent&) = default;

        bool dirty{false};

        void setName(const std::string& in_name) { name = in_name; dirty = true; }
        [[nodiscard]] const std::string& getName() const { return name; }
    };

} // dodoe