// do@Redlive

#include "PaintTilesCommand.h"

#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <algorithm>
#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

} // namespace

PaintTilesCommand::PaintTilesCommand(dodoe::UUID tilemapEntity, dodoe::UUID layerEntity)
    : m_tilemap(tilemapEntity)
    , m_layer(layerEntity)
{}

void PaintTilesCommand::addCell(int x, int y, dodoe::UInt32 before, dodoe::UInt32 after)
{
    m_cells.push_back({x, y, before, after});
}

void PaintTilesCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto layerEntity = ResolveEntity(scene, m_layer);
        if (layerEntity.valid() && layerEntity.hasComponent<dodoe::TileLayerComponent>()) {
            auto& layer = layerEntity.getComponent<dodoe::TileLayerComponent>();
            for (auto& cell : m_cells) {
                layer.setTile(cell.x, cell.y, cell.after);
            }
        }

        auto tilemapEntity = ResolveEntity(scene, m_tilemap);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            tilemapEntity.getComponent<dodoe::TilemapComponent>().dirty = true;
        }
    }

    mirrorDocument(model);
}

void PaintTilesCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto layerEntity = ResolveEntity(scene, m_layer);
        if (layerEntity.valid() && layerEntity.hasComponent<dodoe::TileLayerComponent>()) {
            auto& layer = layerEntity.getComponent<dodoe::TileLayerComponent>();
            for (auto& cell : m_cells) {
                layer.setTile(cell.x, cell.y, cell.before);
            }
        }

        auto tilemapEntity = ResolveEntity(scene, m_tilemap);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            tilemapEntity.getComponent<dodoe::TilemapComponent>().dirty = true;
        }
    }

    mirrorDocument(model);
}

void PaintTilesCommand::mirrorDocument(EditorDocumentModel& model)
{
    EditorEntity* entity = model.findEntity(static_cast<std::uint64_t>(m_layer));
    if (!entity) return;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName != "TileLayerComponent") continue;
        if (!component.value.contains("tiles") || !component.value["tiles"].is_array()) return;
        nlohmann::json& tiles = component.value["tiles"];
        const std::uint64_t width = component.value.value("layer_width", std::uint64_t(0));
        for (const Cell& cell : m_cells) {
            const std::size_t i = static_cast<std::size_t>(cell.y) * width + static_cast<std::size_t>(cell.x);
            if (i < tiles.size()) {
                tiles[i] = cell.after;
            }
        }
        return;
    }
}

std::string PaintTilesCommand::label() const
{
    return "Paint Tiles (" + std::to_string(m_cells.size()) + " cells)";
}

bool PaintTilesCommand::mergeWith(const EditorCommand& next)
{
    auto* n = dynamic_cast<const PaintTilesCommand*>(&next);
    if (!n || n->m_tilemap != m_tilemap || n->m_layer != m_layer) {
        return false;
    }

    for (const auto& cell : n->m_cells) {
        auto it = std::find_if(m_cells.begin(), m_cells.end(),
                               [&](const Cell& c) { return c.x == cell.x && c.y == cell.y; });
        if (it != m_cells.end()) {
            it->after = cell.after;
        } else {
            m_cells.push_back(cell);
        }
    }
    return true;
}

} // namespace cakery
