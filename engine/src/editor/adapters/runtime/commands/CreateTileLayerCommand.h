// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class CreateTileLayerCommand final : public EditorCommand {
public:
    CreateTileLayerCommand(dodoe::UUID tilemap, dodoe::String name, dodoe::UInt32 width, dodoe::UInt32 height);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }
    void setCreatedUuid(dodoe::UUID uuid) { m_createdUuid = uuid; }

private:
    dodoe::UUID m_tilemap;
    dodoe::String m_name;
    dodoe::UInt32 m_width;
    dodoe::UInt32 m_height;
    dodoe::UUID m_createdUuid;
};

} // namespace cakery
