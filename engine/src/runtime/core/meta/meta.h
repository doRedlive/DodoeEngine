//
// Created by GreenMuffin on 2026/1/22.
//

#ifndef DODOE_META_H
#define DODOE_META_H

#include "dopch.h"
#include "entt/entt.hpp"

namespace dodoe {

    inline std::vector<std::string> enumerate_meta_components() {
        std::vector<std::string> result;
        for (const auto meta_range = entt::resolve(); const auto &meta_type: meta_range | std::views::values) {
            const auto& type = meta_type;
            auto name = std::string(type.info().name());
            const auto pos = name.find_last_of(':');

            if (const auto simple_name = pos != std::string::npos ? name.substr(pos + 1) : name; simple_name.ends_with("Component")) {
                result.emplace_back(simple_name);
            }
        }
        std::ranges::sort(result);
        result.erase(std::ranges::unique(result).begin(), result.end());
        return result;
    }

} // dodoe

#endif //DODOE_META_H