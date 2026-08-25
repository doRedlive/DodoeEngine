#include "AssetDatabase.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <unordered_set>

namespace cakery {

namespace {

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string NormalizePath(const std::string& path)
{
    std::filesystem::path normalized(path);
    normalized = normalized.lexically_normal();
    return normalized.generic_string();
}

} // namespace

void AssetDatabase::refresh() {
    m_assets.clear();

    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return;
    if (!am->refreshAssets()) return;
    auto* db = am->getDatabase();
    if (!db) return;

    const dodoe::FsPath assetDir = am->getAssetDir();
    for (const auto& id : db->getAllAssetIDs()) {
        dodoe::AssetMetaData meta = db->getMetaData(id);
        if (meta.source_path.empty()) continue;

        AssetInfo info;
        info.uuid = id.asset_id;
        dodoe::FsPath sp(meta.source_path.c_str());
        info.path = sp.is_absolute()
                        ? std::string(meta.source_path.c_str())
                        : std::string((assetDir / sp).generic_string().c_str());
        info.path = NormalizePath(info.path);
        if (!std::filesystem::is_regular_file(std::filesystem::path(info.path))) {
            continue;
        }
        info.name = meta.name.empty()
            ? std::filesystem::path(info.path).stem().string()
            : std::string(meta.name.c_str());
        info.type = dodoe::Asset::assetTypeToString(meta.type);
        info.extension = Lower(std::filesystem::path(info.path).extension().string());
        info.dirty = am->isAssetDirty(info.uuid);
        for (const auto& dependency : meta.dependencies) {
            info.dependencies.push_back(static_cast<std::uint64_t>(dependency.asset_id));
        }
        m_assets.push_back(info);
    }

    changed.fire();
}

std::vector<AssetDatabase::AssetInfo> AssetDatabase::list(const std::string& filter) const {
    if (filter.empty()) return m_assets;
    const std::string needle = Lower(filter);
    std::vector<AssetInfo> result;
    for (const auto& a : m_assets) {
        const std::string uuid = std::to_string(static_cast<std::uint64_t>(a.uuid));
        if (Lower(a.name).find(needle) != std::string::npos ||
            Lower(a.path).find(needle) != std::string::npos ||
            Lower(a.type).find(needle) != std::string::npos ||
            a.extension.find(needle) != std::string::npos ||
            uuid.find(needle) != std::string::npos) {
            result.push_back(a);
        }
    }
    return result;
}

std::optional<AssetDatabase::AssetInfo> AssetDatabase::findByGuid(dodoe::UUID guid) const {
    for (const auto& a : m_assets) {
        if (a.uuid == guid) return a;
    }
    return std::nullopt;
}

std::optional<AssetDatabase::AssetInfo> AssetDatabase::findByPath(const std::string& path) const
{
    const std::string needle = NormalizePath(path);
    for (const auto& asset : m_assets) {
        if (NormalizePath(asset.path) == needle) {
            return asset;
        }
    }
    return std::nullopt;
}

std::vector<std::string> AssetDatabase::types() const
{
    std::unordered_set<std::string> unique;
    for (const auto& asset : m_assets) {
        if (!asset.type.empty()) {
            unique.insert(asset.type);
        }
    }
    std::vector<std::string> result(unique.begin(), unique.end());
    std::sort(result.begin(), result.end());
    return result;
}

size_t AssetDatabase::saveAllDirty() {
    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return 0;

    size_t saved = 0;
    for (auto it = m_dirty.begin(); it != m_dirty.end();) {
        if (am->saveAsset(*it)) {
            ++saved;
            it = m_dirty.erase(it);
        } else {
            ++it;
        }
    }
    return saved;
}

} // namespace cakery
