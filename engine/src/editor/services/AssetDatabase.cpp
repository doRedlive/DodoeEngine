#include "dopch.h"

#include "AssetDatabase.h"

#include "runtime/core/async/task_scheduler.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <unordered_set>

namespace cakery {

namespace {

String Lower(String value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

String NormalizePath(const String& path)
{
    FsPath normalized(path);
    normalized = normalized.lexically_normal();
    return String(normalized.generic_string());
}

} // namespace

void AssetDatabase::refresh() {
    if (m_refreshing.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    m_progress_done.store(0, std::memory_order_relaxed);
    m_progress_total.store(0, std::memory_order_relaxed);

    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) {
        m_refreshing.store(false, std::memory_order_release);
        return;
    }

    dodoe::RefreshProgressFn progress = [this](Size_t done, Size_t total) {
        m_progress_done.store(done, std::memory_order_relaxed);
        m_progress_total.store(total, std::memory_order_relaxed);
    };

    m_refresh_future = dodoe::TaskScheduler::Self().async(
        [this, am](dodoe::RefreshProgressFn report) {
            return am->scanAssets(std::move(report));
        },
        std::move(progress));
}

void AssetDatabase::finalize() {
    if (!m_refreshing.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (m_refresh_future.valid()) {
        m_refresh_future.wait();
        const dodoe::AssetRefreshResult result = m_refresh_future.get();
        if (auto* am = dodoe::ResourceManager::Self().getAssetManager()) {
            am->applyRefreshResult(result, [this](Size_t done, Size_t total) {
                m_progress_done.store(done, std::memory_order_relaxed);
                m_progress_total.store(total, std::memory_order_relaxed);
            });
        }
    }
    buildFromDatabase();
    changed.fire();
}

void AssetDatabase::cancelAndWait() {
    if (!m_refreshing.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (auto* am = dodoe::ResourceManager::Self().getAssetManager()) {
        am->cancelRefresh();
    }
    if (m_refresh_future.valid()) {
        m_refresh_future.wait();
        m_refresh_future.get();
    }
}

bool AssetDatabase::refreshFinished() const {
    if (!m_refreshing.load(std::memory_order_acquire)) {
        return false;
    }
    if (!m_refresh_future.valid()) {
        return false;
    }
    return m_refresh_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

Pair<Size_t, Size_t> AssetDatabase::progress() const {
    return { m_progress_done.load(std::memory_order_relaxed),
             m_progress_total.load(std::memory_order_relaxed) };
}

void AssetDatabase::buildFromDatabase() {
    m_assets.clear();

    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return;
    auto* db = am->getDatabase();
    if (!db) return;

    const dodoe::FsPath assetDir = am->getAssetDir();
    for (const auto& id : db->getAllAssetIDs()) {
        dodoe::AssetMetaData meta = db->getMetaData(id);
        if (meta.source_path.empty()) continue;

        AssetInfo info;
        info.uuid = id.asset_id;
        dodoe::FsPath sp(meta.source_path);
        info.path = sp.is_absolute()
                        ? String(meta.source_path)
                        : String((assetDir / sp).generic_string());
        info.path = NormalizePath(info.path);
        if (!std::filesystem::is_regular_file(FsPath(info.path))) {
            continue;
        }
        info.name = meta.name.empty()
            ? String(FsPath(info.path).stem().string())
            : String(meta.name);
        info.type = dodoe::Asset::assetTypeToString(meta.type);
        info.extension = Lower(FsPath(info.path).extension().string().c_str());
        info.dirty = am->isAssetDirty(info.uuid);
        for (const auto& dependency : meta.dependencies) {
            info.dependencies.push_back(static_cast<UInt64>(dependency.asset_id));
        }
        m_assets.push_back(info);
    }
}

DynamicArray<AssetDatabase::AssetInfo> AssetDatabase::list(const String& filter) const {
    if (filter.empty()) return m_assets;
    const String needle = Lower(filter);
    DynamicArray<AssetInfo> result;
    for (const auto& a : m_assets) {
        const String uuid = ToString(static_cast<UInt64>(a.uuid));
        if (Lower(a.name).find(needle) != String::npos ||
            Lower(a.path).find(needle) != String::npos ||
            Lower(a.type).find(needle) != String::npos ||
            a.extension.find(needle) != String::npos ||
            uuid.find(needle) != String::npos) {
            result.push_back(a);
        }
    }
    return result;
}

Optional<AssetDatabase::AssetInfo> AssetDatabase::findByGuid(dodoe::UUID guid) const {
    for (const auto& a : m_assets) {
        if (a.uuid == guid) return a;
    }
    return std::nullopt;
}

Optional<AssetDatabase::AssetInfo> AssetDatabase::findByPath(const String& path) const
{
    const String needle = NormalizePath(path);
    for (const auto& asset : m_assets) {
        if (NormalizePath(asset.path) == needle) {
            return asset;
        }
    }
    return std::nullopt;
}

DynamicArray<String> AssetDatabase::types() const
{
    UnorderedSet<String> unique;
    for (const auto& asset : m_assets) {
        if (!asset.type.empty()) {
            unique.insert(asset.type);
        }
    }
    DynamicArray<String> result(unique.begin(), unique.end());
    std::sort(result.begin(), result.end());
    return result;
}

Size_t AssetDatabase::saveAllDirty() {
    auto* am = dodoe::ResourceManager::Self().getAssetManager();
    if (!am) return 0;

    Size_t saved = 0;
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
