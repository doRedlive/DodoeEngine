// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class RemoveTilesetCommand final : public EditorCommand {
public:
    RemoveTilesetCommand(dodoe::UUID tilemap, dodoe::UUID assetId);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    dodoe::UUID m_tilemap;
    dodoe::UUID m_assetId;
    bool m_removed = false;
};

} // namespace cakery
