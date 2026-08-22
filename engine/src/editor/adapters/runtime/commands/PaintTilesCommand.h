// do@Redlive

#pragma once

#include "core/commands/EditorCommand.h"
#include "runtime/core/utils/uuid.h"

#include <string>
#include <vector>

namespace cakery {

class PaintTilesCommand final : public EditorCommand {
public:
    struct Cell { int x, y; dodoe::UInt32 before, after; };

    PaintTilesCommand(dodoe::UUID tilemapEntity, dodoe::UUID layerEntity);

    void addCell(int x, int y, dodoe::UInt32 before, dodoe::UInt32 after);
    bool empty() const { return m_cells.empty(); }

    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    void mirrorDocument(EditorDocumentModel& model);

    dodoe::UUID m_tilemap;
    dodoe::UUID m_layer;
    std::vector<Cell> m_cells;
};

} // namespace cakery
