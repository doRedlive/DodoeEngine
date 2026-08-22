// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class CreateTilemapCommand final : public EditorCommand {
public:
    CreateTilemapCommand(dodoe::String name, dodoe::UInt32 width, dodoe::UInt32 height,
                         dodoe::UInt32 tileWidth = 16, dodoe::UInt32 tileHeight = 16);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }

private:
    dodoe::String m_name;
    dodoe::UInt32 m_width;
    dodoe::UInt32 m_height;
    dodoe::UInt32 m_tileWidth;
    dodoe::UInt32 m_tileHeight;
    dodoe::UUID m_createdUuid;
    dodoe::UUID m_layerUuid;
};

} // namespace cakery
