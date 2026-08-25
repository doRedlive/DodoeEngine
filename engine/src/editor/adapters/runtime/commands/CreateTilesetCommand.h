// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class CreateTilesetCommand final : public EditorCommand {
public:
    CreateTilesetCommand(dodoe::UUID tilemap, dodoe::String imagePath, dodoe::UInt32 tileWidth,
                         dodoe::UInt32 tileHeight, dodoe::UInt32 margin, dodoe::UInt32 spacing);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID createdAssetId() const { return m_createdAssetId; }

private:
    dodoe::UUID m_tilemap;
    dodoe::String m_imagePath;
    dodoe::UInt32 m_tileWidth;
    dodoe::UInt32 m_tileHeight;
    dodoe::UInt32 m_margin;
    dodoe::UInt32 m_spacing;
    dodoe::UUID m_createdAssetId;
    bool m_created = false;
};

} // namespace cakery
