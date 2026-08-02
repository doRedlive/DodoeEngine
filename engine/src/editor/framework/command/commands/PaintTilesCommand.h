// do@Redlive

#pragma once

#include "framework/command/ICommand.h"
#include "runtime/core/utils/uuid.h"
#include <vector>
#include <string>

namespace cakery {

class PaintTilesCommand : public ICommand {
public:
    struct Cell { int x, y; dodoe::UInt32 before, after; };

    PaintTilesCommand(dodoe::UUID tilemapEntity, dodoe::UUID layerEntity);

    void addCell(int x, int y, dodoe::UInt32 before, dodoe::UInt32 after);
    bool empty() const { return m_cells.empty(); }

    bool execute(EditorContext& ctx) override;
    void undo(EditorContext& ctx) override;
    std::string label() const override;
    bool mergeWith(const ICommand& next) override;

private:
    dodoe::UUID m_tilemap;
    dodoe::UUID m_layer;
    std::vector<Cell> m_cells;
};

} // namespace cakery
