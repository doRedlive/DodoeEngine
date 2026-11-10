// do@Redlive

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "runtime/core/utils/json.h"

namespace dodoe::config {

    inline Json LoadJsonFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            return Json();
        }
        std::ifstream file(path);
        if (!file.is_open()) {
            return Json();
        }
        try {
            Json j;
            file >> j;
            return j;
        } catch (const std::exception&) {
            return Json();
        }
    }

    inline void MergeOverride(Json& base, const std::filesystem::path& dir, const std::string& filename) {
        const std::filesystem::path path = dir / filename;
        if (!std::filesystem::exists(path)) {
            return;
        }
        Json override_json = LoadJsonFile(path);
        if (!override_json.is_object()) {
            return;
        }
        if (!base.is_object()) {
            base = std::move(override_json);
            return;
        }
        base.merge_patch(override_json);
    }

    inline Json LoadLayered(const std::filesystem::path& builtin_dir,
                                      const std::filesystem::path& project_dir,
                                      const std::filesystem::path& user_dir,
                                      const std::string& filename) {
        Json merged = LoadJsonFile(builtin_dir / filename);
        if (!project_dir.empty()) {
            MergeOverride(merged, project_dir, filename);
        }
        if (!user_dir.empty()) {
            MergeOverride(merged, user_dir, filename);
        }
        if (!merged.is_object()) {
            merged = Json::object();
        }
        return merged;
    }

} // namespace dodoe::config
