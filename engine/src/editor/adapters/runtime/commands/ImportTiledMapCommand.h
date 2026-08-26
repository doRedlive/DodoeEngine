// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cakery {

class ImportTiledMapCommand final : public EditorCommand {
public:
    ImportTiledMapCommand(dodoe::String name, dodoe::UUID tiledMapAssetId, nlohmann::json position);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }

private:
    dodoe::String m_name;
    dodoe::UUID m_tiledMapAssetId;
    nlohmann::json m_position;
    dodoe::UUID m_createdUuid;
    std::vector<dodoe::UUID> m_layerUuids;
    std::vector<dodoe::UUID> m_tilesetAssetIds;
    bool m_created = false;
};

} // namespace cakery
