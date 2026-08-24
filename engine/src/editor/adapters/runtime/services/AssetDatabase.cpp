#include "AssetDatabase.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"

namespace cakery {

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
        info.type = dodoe::Asset::assetTypeToString(meta.type);
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
