// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/container/containers.h"
#include <string>

namespace cakery {

class CreateTileLayerCommand : public ICommand {
public:
    CreateTileLayerCommand(dodoe::UUID tilemap, dodoe::String name,
                           dodoe::UInt32 width, dodoe::UInt32 height);

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;

    dodoe::UUID created() const { return m_createdUuid; }

private:
    dodoe::UUID m_tilemap;
    dodoe::String m_name;
    dodoe::UInt32 m_width;
    dodoe::UInt32 m_height;
    dodoe::UUID m_createdUuid;
};

} // namespace cakery
