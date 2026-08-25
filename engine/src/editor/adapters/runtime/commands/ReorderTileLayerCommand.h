// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>

namespace cakery {

class ReorderTileLayerCommand final : public EditorCommand {
public:
    ReorderTileLayerCommand(dodoe::UUID tilemap, dodoe::UUID layer, bool moveUp);

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    void swapInScene(EditorDocumentModel& model, bool reverse);

    dodoe::UUID m_tilemap;
    dodoe::UUID m_layer;
    bool m_moveUp;
    bool m_swapped = false;
    dodoe::UUID m_otherLayer;
};

} // namespace cakery
