#include "AssetDatabase.h"
#include "framework/EditorContext.h"

#include "runtime/core/project/project.h"

#include <filesystem>

namespace cakery {

void AssetDatabase::refresh() {
    m_assets.clear();

    auto proj = dodoe::Project::ActiveProject();
    if (!proj) return;

    std::string assetPath = proj->config().project_path.string();
    if (assetPath.empty()) return;

    namespace fs = std::filesystem;
    if (!fs::exists(assetPath)) return;

    for (auto& entry : fs::recursive_directory_iterator(assetPath)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::string type;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") type = "texture";
        else if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") type = "mesh";
        else if (ext == ".tmj" || ext == ".tsj") type = "tilemap";
        else if (ext == ".scene") type = "scene";
        else type = "unknown";

        AssetInfo info;
        info.path = entry.path().string();
        info.type = type;
        m_assets.push_back(info);
    }

    changed.fire();
}

std::vector<AssetDatabase::AssetInfo> AssetDatabase::list(const std::string& filter) const {
    if (filter.empty()) return m_assets;
    std::vector<AssetInfo> result;
    for (auto& a : m_assets) {
        if (a.type == filter) result.push_back(a);
    }
    return result;
}

std::optional<AssetDatabase::AssetInfo> AssetDatabase::findByGuid(dodoe::UUID guid) const {
    for (auto& a : m_assets) {
        if (a.uuid == guid) return a;
    }
    return std::nullopt;
}

} // namespace cakery
