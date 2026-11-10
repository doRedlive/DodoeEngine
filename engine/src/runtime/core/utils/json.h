// do@Redlive

#pragma once

#include "runtime/core/container/containers.h"

#include <nlohmann/json.hpp>

namespace dodoe {

    using Json = nlohmann::json;

    inline String ToString(const Json& value) {
        const std::string text = value.dump();
        return String(text.data(), text.size());
    }

    inline String ToString(const Json& value, int indent) {
        const std::string text = value.dump(indent);
        return String(text.data(), text.size());
    }

} // dodoe

namespace nlohmann {
    template <>
    struct adl_serializer<dodoe::StringBase> {
        static void to_json(json& j, const dodoe::StringBase& s) {
            j = std::string(s.data(), s.size());
        }
        static void from_json(const json& j, dodoe::StringBase& s) {
            auto std_s = j.get<std::string>();
            s = dodoe::StringBase(std_s.data(), std_s.size());
        }
    };

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

using dodoe::Json;
