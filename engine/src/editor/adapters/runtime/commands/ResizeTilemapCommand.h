// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cakery {

class ResizeTilemapCommand final : public EditorCommand {
public:
    ResizeTilemapCommand(dodoe::UUID tilemap, dodoe::UInt32 newWidth, dodoe::UInt32 newHeight);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    struct LayerSnapshot {
        dodoe::UUID uuid;
        dodoe::UInt32 width;
        dodoe::UInt32 height;
        std::vector<dodoe::UInt32> tiles;
    };

    void applyDims(EditorDocumentModel& model, dodoe::UInt32 width, dodoe::UInt32 height);

    dodoe::UUID m_tilemap;
    dodoe::UInt32 m_newWidth;
    dodoe::UInt32 m_newHeight;
    dodoe::UInt32 m_oldWidth;
    dodoe::UInt32 m_oldHeight;
    std::vector<LayerSnapshot> m_layers;
    bool m_done = false;
};

} // namespace cakery
