// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <nlohmann/json.hpp>
#include <string>

namespace cakery {

class CreateTilemapWithTilesetCommand final : public EditorCommand {
public:
    CreateTilemapWithTilesetCommand(dodoe::String name, dodoe::UUID tilesetAssetId,
                                    nlohmann::json position);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }

private:
    dodoe::String m_name;
    dodoe::UUID m_tilesetAssetId;
    nlohmann::json m_position;
    dodoe::UUID m_createdUuid;
    dodoe::UUID m_layerUuid;
    bool m_created = false;
};

} // namespace cakery
