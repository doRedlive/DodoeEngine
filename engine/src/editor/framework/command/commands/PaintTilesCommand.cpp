// do@Redlive

#include "PaintTilesCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/log/log_system.h"

#include <unordered_map>

namespace cakery {

PaintTilesCommand::PaintTilesCommand(dodoe::Uuid tilemapEntity, dodoe::Uuid layerEntity)
    : m_tilemap(tilemapEntity)
    , m_layer(layerEntity)
{}

void PaintTilesCommand::addCell(int x, int y, dodoe::UInt32 before, dodoe::UInt32 after)
{
    m_cells.push_back({x, y, before, after});
}

bool PaintTilesCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto layerEntity = ResolveEntity(scene, m_layer);
    if (!layerEntity.valid()) return false;

    if (!layerEntity.hasComponent<dodoe::TileLayerComponent>()) return false;

    auto& layer = layerEntity.getComponent<dodoe::TileLayerComponent>();

    for (auto& cell : m_cells) {
        layer.setTile(cell.x, cell.y, cell.after);
    }

    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
        tilemapEntity.getComponent<dodoe::TilemapComponent>().dirty = true;
    }

    return true;
}

void PaintTilesCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto layerEntity = ResolveEntity(scene, m_layer);
    if (!layerEntity.valid()) return;

    if (!layerEntity.hasComponent<dodoe::TileLayerComponent>()) return;

    auto& layer = layerEntity.getComponent<dodoe::TileLayerComponent>();

    for (auto& cell : m_cells) {
        layer.setTile(cell.x, cell.y, cell.before);
    }

    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
        tilemapEntity.getComponent<dodoe::TilemapComponent>().dirty = true;
    }
}

std::string PaintTilesCommand::label() const
{
    return "Paint Tiles (" + std::to_string(m_cells.size()) + " cells)";
}

bool PaintTilesCommand::mergeWith(const ICommand& next)
{
    auto* n = dynamic_cast<const PaintTilesCommand*>(&next);
    if (!n || n->m_tilemap != m_tilemap || n->m_layer != m_layer) {
        return false;
    }

    m_cells.insert(m_cells.end(), n->m_cells.begin(), n->m_cells.end());
    return true;
}

} // namespace cakery
