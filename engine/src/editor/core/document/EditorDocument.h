// do@Redlive

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cakery {

struct EditorComponent {
    std::string typeName;
    nlohmann::json value = nlohmann::json::object();
};

struct EditorEntity {
    std::uint64_t uuid = 0;
    std::uint64_t parent = 0;
    std::string name;
    std::vector<EditorComponent> nativeComponents;
    std::vector<EditorComponent> managedComponents;
};

struct EditorDocument {
    std::string name;
    std::vector<EditorEntity> entities;
};

} // namespace cakery
