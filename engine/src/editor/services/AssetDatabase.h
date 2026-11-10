#include "dopch.h"

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <utility>
#include "core/Signal.h"
#include "runtime/core/utils/uuid.h"

namespace cakery {

class AssetDatabase {
public:
    struct AssetInfo {
        dodoe::UUID uuid;
        String path;
        String name;
        String type;
        String extension;
        bool dirty = false;
        DynamicArray<UInt64> dependencies;
    };

    void refresh();
    void finalize();
    void cancelAndWait();
    // Matches name, normalized path, type, extension, or GUID substring.
    DynamicArray<AssetInfo> list(const String& filter = "") const;
    Optional<AssetInfo> findByGuid(dodoe::UUID guid) const;
    Optional<AssetInfo> findByPath(const String& path) const;
    DynamicArray<String> types() const;

    void markDirty(dodoe::UUID uuid) { m_dirty.insert(uuid); }
    [[nodiscard]] bool isDirty(dodoe::UUID uuid) const { return m_dirty.find(uuid) != m_dirty.end(); }
    Size_t saveAllDirty();

    [[nodiscard]] bool refreshPending() const { return m_refreshing.load(std::memory_order_acquire); }
    [[nodiscard]] bool refreshFinished() const;
    [[nodiscard]] Pair<Size_t, Size_t> progress() const;

    Signal<> changed;

private:
    void buildFromDatabase();

    DynamicArray<AssetInfo> m_assets;
    UnorderedSet<dodoe::UUID> m_dirty;
    std::atomic<bool> m_refreshing{false};
    std::future<bool> m_refresh_future;
    std::atomic<Size_t> m_progress_done{0};
    std::atomic<Size_t> m_progress_total{0};
};

} // namespace cakery
