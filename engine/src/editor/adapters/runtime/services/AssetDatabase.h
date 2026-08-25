#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include "core/Signal.h"
#include "runtime/core/utils/uuid.h"

namespace cakery {

class AssetDatabase {
public:
    struct AssetInfo {
        dodoe::UUID uuid;
        std::string path;
        std::string name;
        std::string type;
        std::string extension;
        bool dirty = false;
        std::vector<std::uint64_t> dependencies;
    };

    void refresh();
    // Matches name, normalized path, type, extension, or GUID substring.
    std::vector<AssetInfo> list(const std::string& filter = "") const;
    std::optional<AssetInfo> findByGuid(dodoe::UUID guid) const;
    std::optional<AssetInfo> findByPath(const std::string& path) const;
    std::vector<std::string> types() const;

    void markDirty(dodoe::UUID uuid) { m_dirty.insert(uuid); }
    [[nodiscard]] bool isDirty(dodoe::UUID uuid) const { return m_dirty.find(uuid) != m_dirty.end(); }
    size_t saveAllDirty();

    Signal<> changed;

private:
    std::vector<AssetInfo> m_assets;
    std::unordered_set<dodoe::UUID> m_dirty;
};

} // namespace cakery
