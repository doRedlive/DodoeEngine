// do@Redlive

#pragma once

#include "EditorDocument.h"

#include <filesystem>

#include <nlohmann/json.hpp>

namespace cakery {

class EditorDocumentSerializer {
public:
    static nlohmann::json toJson(const EditorDocument& document);
    static bool fromJson(const nlohmann::json& json, EditorDocument& outDocument);
    static bool load(const std::filesystem::path& path, EditorDocument& outDocument);
    static bool save(const EditorDocument& document, const std::filesystem::path& path);
};

} // namespace cakery
