// do@Redlive

#include "TilePaintService.h"

#include "adapters/runtime/services/UuidResolve.h"
#include "adapters/runtime/commands/CreateTileLayerCommand.h"
#include "adapters/runtime/commands/CreateTilemapCommand.h"
#include "adapters/runtime/commands/PaintTilesCommand.h"
#include "core/console/CommandRegistry.h"
#include "core/EditorSession.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

dodoe::UInt32 readTile(dodoe::UUID layerEntity, int x, int y) {
    auto* scene = ActiveScene();
    if (!scene) return 0;
    auto entity = ResolveEntity(scene, layerEntity);
    if (!entity.valid()) return 0;
    auto* layer = entity.hasComponent<dodoe::TileLayerComponent>()
                      ? &entity.getComponent<dodoe::TileLayerComponent>()
                      : nullptr;
    if (!layer) return 0;
    return layer->getTile(x, y);
}

void applyBrush(PaintTilesCommand* cmd, dodoe::UUID layerEntity, int cx, int cy, const TileBrush& brush) {
    for (int by = 0; by < brush.h; ++by) {
        for (int bx = 0; bx < brush.w; ++bx) {
            int gx = cx + bx;
            int gy = cy + by;
            dodoe::UInt32 gid = brush.gids[by * brush.w + bx];
            dodoe::UInt32 before = readTile(layerEntity, gx, gy);
            if (before != gid) {
                cmd->addCell(gx, gy, before, gid);
            }
        }
    }
}

void TraceLine(PaintTilesCommand* cmd, dodoe::UUID layerEntity, int x0, int y0, int x1, int y1,
               const TileBrush& brush) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        applyBrush(cmd, layerEntity, x0, y0, brush);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

} // namespace

void TilePaintService::setActiveEntity(dodoe::UUID entity) {
    auto* scene = ActiveScene();
    if (!scene) return;
    auto e = ResolveEntity(scene, entity);
    if (!e.valid()) return;

    if (e.hasComponent<dodoe::TilemapComponent>()) {
        setActiveTilemap(entity);
        if (!m_layer.isValid() && e.hasComponent<dodoe::HierarchyComponent>()) {
            for (auto child : e.getComponent<dodoe::HierarchyComponent>().children) {
                if (child.valid() && child.hasComponent<dodoe::TileLayerComponent>()) {
                    setActiveLayer(child.uuid());
                    break;
                }
            }
        }
    } else if (e.hasComponent<dodoe::TileLayerComponent>()) {
        setActiveLayer(entity);
        if (!m_tilemap.isValid() && e.hasComponent<dodoe::HierarchyComponent>()) {
            dodoe::UUID parentUuid = e.getComponent<dodoe::HierarchyComponent>().parent_uuid;
            if (parentUuid.isValid()) {
                auto parent = ResolveEntity(scene, parentUuid);
                if (parent.valid() && parent.hasComponent<dodoe::TilemapComponent>()) {
                    setActiveTilemap(parentUuid);
                }
            }
        }
    }
}

void TilePaintService::onCellDown(int cx, int cy) {
    if (!hasTarget() || m_tool == TileTool::Select) return;

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        m_anchorX = m_lastX = cx;
        m_anchorY = m_lastY = cy;
        m_hasAnchor = true;
        return;
    }

    m_session.history().beginMerge();

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(cmd.get(), m_layer, cx, cy, m_brush);
    } else if (m_tool == TileTool::Erase) {
        TileBrush eraser;
        eraser.w = m_brush.w;
        eraser.h = m_brush.h;
        eraser.gids.assign(eraser.w * eraser.h, 0);
        applyBrush(cmd.get(), m_layer, cx, cy, eraser);
    } else if (m_tool == TileTool::Picker) {
        dodoe::UInt32 gid = readTile(m_layer, cx, cy);
        if (gid > 0) {
            m_brush.w = 1;
            m_brush.h = 1;
            m_brush.gids = {gid};
        }
        m_session.history().endMerge();
        return;
    } else if (m_tool == TileTool::Fill) {
        dodoe::UInt32 targetGid = readTile(m_layer, cx, cy);
        dodoe::UInt32 fillGid = m_brush.gids[0];
        if (fillGid == targetGid) {
            m_session.history().endMerge();
            return;
        }
        auto* scene = ActiveScene();
        if (!scene) { m_session.history().endMerge(); return; }
        auto entity = ResolveEntity(scene, m_layer);
        if (!entity.valid()) { m_session.history().endMerge(); return; }
        auto* layer = entity.hasComponent<dodoe::TileLayerComponent>()
                          ? &entity.getComponent<dodoe::TileLayerComponent>()
                          : nullptr;
        if (!layer) { m_session.history().endMerge(); return; }
        const dodoe::Int32 w = static_cast<dodoe::Int32>(layer->layer_width);
        const dodoe::Int32 h = static_cast<dodoe::Int32>(layer->layer_height);
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) { m_session.history().endMerge(); return; }

        struct CellPos { dodoe::Int32 x, y; };
        std::vector<std::uint8_t> visited(static_cast<std::size_t>(w) * h, 0);
        std::vector<CellPos> queue;
        queue.push_back({cx, cy});
        visited[static_cast<std::size_t>(cy) * w + cx] = 1;

        const dodoe::Int32 dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!queue.empty()) {
            CellPos cur = queue.back();
            queue.pop_back();
            if (layer->getTile(cur.x, cur.y) != targetGid) continue;
            cmd->addCell(cur.x, cur.y, targetGid, fillGid);
            for (const auto& d : dirs) {
                dodoe::Int32 nx = cur.x + d[0];
                dodoe::Int32 ny = cur.y + d[1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                if (visited[static_cast<std::size_t>(ny) * w + nx]) continue;
                visited[static_cast<std::size_t>(ny) * w + nx] = 1;
                if (layer->getTile(nx, ny) == targetGid) {
                    queue.push_back({nx, ny});
                }
            }
        }
    }

    if (!cmd->empty()) {
        m_session.history().execute(std::move(cmd), m_session.documentModel());
        m_session.notifyDocumentChanged();
    } else {
        m_session.history().endMerge();
    }
}

void TilePaintService::onCellDrag(int cx, int cy) {
    if (!hasTarget() || m_tool == TileTool::Select) return;

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        m_lastX = cx;
        m_lastY = cy;
        return;
    }

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(cmd.get(), m_layer, cx, cy, m_brush);
    } else if (m_tool == TileTool::Erase) {
        TileBrush eraser;
        eraser.w = m_brush.w;
        eraser.h = m_brush.h;
        eraser.gids.assign(eraser.w * eraser.h, 0);
        applyBrush(cmd.get(), m_layer, cx, cy, eraser);
    }

    if (!cmd->empty()) {
        m_session.history().execute(std::move(cmd), m_session.documentModel());
        m_session.notifyDocumentChanged();
    }
}

void TilePaintService::onCellUp() {
    if (!hasTarget() || m_tool == TileTool::Select) return;

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        if (m_hasAnchor) {
            auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);
            if (m_tool == TileTool::Line) {
                TraceLine(cmd.get(), m_layer, m_anchorX, m_anchorY, m_lastX, m_lastY, m_brush);
            } else {
                int x0 = m_anchorX < m_lastX ? m_anchorX : m_lastX;
                int x1 = m_anchorX > m_lastX ? m_anchorX : m_lastX;
                int y0 = m_anchorY < m_lastY ? m_anchorY : m_lastY;
                int y1 = m_anchorY > m_lastY ? m_anchorY : m_lastY;
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        applyBrush(cmd.get(), m_layer, x, y, m_brush);
                    }
                }
            }
            if (!cmd->empty()) {
                m_session.history().execute(std::move(cmd), m_session.documentModel());
                m_session.notifyDocumentChanged();
            }
        }
        m_hasAnchor = false;
        return;
    }

    m_session.history().endMerge();
}

void TilePaintService::RegisterCommands()
{
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& reg = CommandRegistry::self();

    reg.add({"tilemap.create", "Create a new tilemap GameObject with a default layer",
             "tilemap.create name=<string> width=<int> height=<int>",
             {{"name", "string", "Tilemap name", true},
              {"width", "int", "Map width in tiles", true},
              {"height", "int", "Map height in tiles", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 auto* scene = ActiveScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 std::string name = args.named.value("name",
                     args.positional.empty() ? std::string("Tilemap") : args.positional[0]);
                 std::string wStr = args.named.value("width",
                     args.positional.size() > 1 ? args.positional[1] : std::string());
                 std::string hStr = args.named.value("height",
                     args.positional.size() > 2 ? args.positional[2] : std::string());
                 if (wStr.empty() || hStr.empty()) {
                     return CommandResult::Err("Usage: tilemap.create name=<string> width=<int> height=<int>");
                 }
                 char* wEnd = nullptr;
                 char* hEnd = nullptr;
                 long w = std::strtol(wStr.c_str(), &wEnd, 10);
                 long h = std::strtol(hStr.c_str(), &hEnd, 10);
                 if (!wEnd || *wEnd != '\0' || !hEnd || *hEnd != '\0' || w <= 0 || h <= 0) {
                     return CommandResult::Err("width/height must be positive integers");
                 }
                 auto cmd = std::make_unique<CreateTilemapCommand>(
                     dodoe::String(name.data(), name.size()),
                     static_cast<dodoe::UInt32>(w), static_cast<dodoe::UInt32>(h));
                 auto* executed = session.history().execute(std::move(cmd), session.documentModel());
                 if (!executed) return CommandResult::Err("Failed to create tilemap");
                 session.notifyDocumentChanged();
                 auto* created = static_cast<CreateTilemapCommand*>(executed);
                 return CommandResult::Ok("Created tilemap '" + name + "' ("
                                          + std::to_string(static_cast<uint64_t>(created->created())) + ")");
             }});

    reg.add({"tilemap.layer", "Create a new tile layer under a tilemap",
             "tilemap.layer <tilemap_uuid> name=<string> width=<int> height=<int>",
             {{"tilemap", "uuid", "Parent tilemap GameObject UUID", true},
              {"name", "string", "Layer name", true},
              {"width", "int", "Layer width in tiles", true},
              {"height", "int", "Layer height in tiles", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 auto* scene = ActiveScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 dodoe::UUID tilemapUuid;
                 if (!args.positional.empty()) {
                     tilemapUuid = dodoe::UUID::FromString(
                         dodoe::String(args.positional[0].data(), args.positional[0].size()));
                 } else if (args.named.contains("tilemap")) {
                     const auto tilemapId = args.named["tilemap"].get<std::string>();
                     tilemapUuid = dodoe::UUID::FromString(
                         dodoe::String(tilemapId.data(), tilemapId.size()));
                 }
                 if (!tilemapUuid.isValid()) return CommandResult::Err("No tilemap UUID specified");
                 auto tilemapEntity = ResolveEntity(scene, tilemapUuid);
                 if (!tilemapEntity.valid()) return CommandResult::Err("Tilemap GameObject not found");
                 if (!tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
                     return CommandResult::Err("GameObject is not a tilemap");
                 }
                 std::string name = args.named.value("name",
                     args.positional.size() > 1 ? args.positional[1] : std::string("Layer"));
                 std::string wStr = args.named.value("width",
                     args.positional.size() > 2 ? args.positional[2] : std::string());
                 std::string hStr = args.named.value("height",
                     args.positional.size() > 3 ? args.positional[3] : std::string());
                 if (wStr.empty() || hStr.empty()) {
                     return CommandResult::Err("Usage: tilemap.layer <tilemap_uuid> name=<string> width=<int> height=<int>");
                 }
                 char* wEnd = nullptr;
                 char* hEnd = nullptr;
                 long w = std::strtol(wStr.c_str(), &wEnd, 10);
                 long h = std::strtol(hStr.c_str(), &hEnd, 10);
                 if (!wEnd || *wEnd != '\0' || !hEnd || *hEnd != '\0' || w <= 0 || h <= 0) {
                     return CommandResult::Err("width/height must be positive integers");
                 }
                 auto cmd = std::make_unique<CreateTileLayerCommand>(
                     tilemapUuid, dodoe::String(name.data(), name.size()),
                     static_cast<dodoe::UInt32>(w), static_cast<dodoe::UInt32>(h));
                 auto* executed = session.history().execute(std::move(cmd), session.documentModel());
                 if (!executed) return CommandResult::Err("Failed to create layer");
                 session.notifyDocumentChanged();
                 auto* created = static_cast<CreateTileLayerCommand*>(executed);
                 return CommandResult::Ok("Created layer '" + name + "' on tilemap ("
                                          + std::to_string(static_cast<uint64_t>(created->created())) + ")");
             }});
}

} // namespace cakery
