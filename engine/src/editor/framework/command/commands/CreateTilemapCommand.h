// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/container/containers.h"
#include <string>

namespace cakery {

class CreateTilemapCommand : public ICommand {
public:
    CreateTilemapCommand(dodoe::String name, dodoe::UInt32 width, dodoe::UInt32 height,
                         dodoe::UInt32 tileWidth = 16, dodoe::UInt32 tileHeight = 16);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
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
