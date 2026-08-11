#pragma once

#include <string>
#include <vector>
#include <optional>
#include "framework/core/Signal.h"
#include "runtime/core/utils/uuid.h"

namespace cakery {

class EditorContext;

class AssetDatabase {
public:
    explicit AssetDatabase(EditorContext& ctx) : m_ctx(ctx) {}

    struct AssetInfo {
        dodoe::UUID uuid;
        std::string path;
        std::string type;
    };

    void refresh();
    std::vector<AssetInfo> list(const std::string& filter = "") const;
    std::optional<AssetInfo> findByGuid(dodoe::UUID guid) const;

    Signal<> changed;

private:
    EditorContext& m_ctx;
    std::vector<AssetInfo> m_assets;
};

} // namespace cakery
