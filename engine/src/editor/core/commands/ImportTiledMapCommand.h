// do@Redlive
#pragma once
#include "dopch.h"

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>
#include <vector>

namespace cakery {

class ImportTiledMapCommand final : public EditorCommand {
public:
    ImportTiledMapCommand(dodoe::String name, dodoe::UUID tiledMapAssetId, Json position);

    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

    dodoe::UUID created() const { return m_createdUuid; }

private:
    dodoe::String m_name;
    dodoe::UUID m_tiledMapAssetId;
    Json m_position;
    dodoe::UUID m_createdUuid;
    DynamicArray<dodoe::UUID> m_layerUuids;
    DynamicArray<dodoe::UUID> m_tilesetAssetIds;
    bool m_created = false;
};

} // namespace cakery
