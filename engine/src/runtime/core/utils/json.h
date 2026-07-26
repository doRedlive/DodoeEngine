// do@Redlive

#pragma once

#include "dopch.h"

#include <nlohmann/json.hpp>

namespace dodoe {

    using Json = nlohmann::json;

} // dodoe

namespace nlohmann {
    template <>
    struct adl_serializer<dodoe::String> {
        static void to_json(json& j, const dodoe::String& s) {
            j = std::string(s.data(), s.size());
        }
        static void from_json(const json& j, dodoe::String& s) {
            auto std_s = j.get<std::string>();
            s = dodoe::String(std_s.data(), std_s.size());
        }
    };
} // namespace nlohmann
