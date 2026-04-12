// do->GreenMuffin

#pragma once

#include "dopch.h"

#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    struct TagComponent {
        identifier id;
        std::string tag;

        TagComponent() : id(string2hash("default")), tag("default") { }
        TagComponent(const std::string& tag) : id(string2hash(tag)), tag(tag) { }

        bool dirty{false};

        void setTag(const std::string& in_tag) { tag = in_tag; id = string2hash(tag); dirty = true; } 
        [[nodiscard]] const std::string& getTag() const { return tag; }
    };

} // dodoe