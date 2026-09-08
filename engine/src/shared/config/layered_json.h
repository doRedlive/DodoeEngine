// do@Redlive

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace dodoe::config {

    inline nlohmann::json LoadJsonFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            return nlohmann::json();
        }
        std::ifstream file(path);
        if (!file.is_open()) {
            return nlohmann::json();
        }
        try {
            nlohmann::json j;
            file >> j;
            return j;
        } catch (const std::exception&) {
            return nlohmann::json();
        }
    }

    inline void MergeOverride(nlohmann::json& base, const std::filesystem::path& dir, const std::string& filename) {
        const std::filesystem::path path = dir / filename;
        if (!std::filesystem::exists(path)) {
            return;
        }
        nlohmann::json override_json = LoadJsonFile(path);
        if (!override_json.is_object()) {
            return;
        }
        if (!base.is_object()) {
            base = std::move(override_json);
            return;
        }
        base.merge_patch(override_json);
    }

    inline nlohmann::json LoadLayered(const std::filesystem::path& builtin_dir,
                                      const std::filesystem::path& project_dir,
                                      const std::filesystem::path& user_dir,
                                      const std::string& filename) {
        nlohmann::json merged = LoadJsonFile(builtin_dir / filename);
        if (!project_dir.empty()) {
            MergeOverride(merged, project_dir, filename);
        }
        if (!user_dir.empty()) {
            MergeOverride(merged, user_dir, filename);
        }
        if (!merged.is_object()) {
            merged = nlohmann::json::object();
        }
        return merged;
    }

} // namespace dodoe::config
