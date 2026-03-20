//
// Created by GreenMuffin on 2026/1/22.
//

#ifndef DODOE_COMMON_H
#define DODOE_COMMON_H

#include "dopch.h"
#include "entt/entt.hpp"

namespace dodoe {

    inline uint32_t string2hash(const std::string& str) {
        return entt::hashed_string{ str.c_str() }.value();
    }

    inline void name_remove_namespace(std::string& name) {
        if (const auto pos = name.find_last_of(':'); pos != std::string::npos) {
            name = name.substr(pos + 1);
        }
    }


} // dodoe

#endif //DODOE_COMMON_H