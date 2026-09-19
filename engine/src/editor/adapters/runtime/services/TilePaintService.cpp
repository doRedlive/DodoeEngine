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
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

namespace {

std::mt19937& Rng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

dodoe::TileLayerComponent* ActiveLayer(dodoe::UUID layerEntity) {
    auto* scene = ActiveScene();
    if (!scene) return nullptr;
    auto entity = ResolveEntity(scene, layerEntity);
    if (!entity.valid()) return nullptr;
    return entity.hasComponent<dodoe::TileLayerComponent>()
               ? &entity.getComponent<dodoe::TileLayerComponent>()
               : nullptr;
}

bool CellInLayer(const dodoe::TileLayerComponent& layer, int x, int y) {
    return x >= 0 && y >= 0 &&
           x < static_cast<int>(layer.layer_width) &&
           y < static_cast<int>(layer.layer_height);
}

dodoe::UInt32 readTile(dodoe::UUID layerEntity, int x, int y) {
    auto* layer = ActiveLayer(layerEntity);
    if (!layer) return 0;
    return layer->getTile(x, y);
}

dodoe::UInt32 RandomBrushGid(const TileBrush& brush) {
    dodoe::UInt32 picked = 0;
    std::size_t nonzero = 0;
    for (dodoe::UInt32 gid : brush.gids) {
        if (gid == 0) continue;
        ++nonzero;
        std::uniform_int_distribution<std::size_t> dist(1, nonzero);
        if (dist(Rng()) == 1) {
            picked = gid;
        }
    }
    return picked;
}

void applyBrush(PaintTilesCommand* cmd, dodoe::UUID layerEntity, int cx, int cy,
                const TileBrush& brush, bool random = false) {
    for (int by = 0; by < brush.h; ++by) {
        for (int bx = 0; bx < brush.w; ++bx) {
            int gx = cx + bx;
            int gy = cy + by;
            dodoe::UInt32 gid = random
                ? RandomBrushGid(brush)
                : brush.gids[static_cast<std::size_t>(by * brush.w + bx)];
            dodoe::UInt32 before = readTile(layerEntity, gx, gy);
            if (before != gid) {
                cmd->addCell(gx, gy, before, gid);
            }
        }
    }
}

void TraceLine(PaintTilesCommand* cmd, dodoe::UUID layerEntity, int x0, int y0, int x1, int y1,
               const TileBrush& brush, bool random = false) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        applyBrush(cmd, layerEntity, x0, y0, brush, random);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

dodoe::UInt32 RotateGidCW(dodoe::UInt32 gid) {
    const dodoe::UInt32 id = dodoe::TileIdOfGid(gid);
    const bool h = (gid & dodoe::kTileFlipHorizontal) != 0;
    const bool v = (gid & dodoe::kTileFlipVertical) != 0;
    const bool d = (gid & dodoe::kTileFlipDiagonal) != 0;
    bool nh, nv, nd;
    switch ((h ? 4 : 0) | (v ? 2 : 0) | (d ? 1 : 0)) {
    case 0: nh = true;  nv = false; nd = true;  break;
    case 1: nh = true;  nv = false; nd = false; break;
    case 2: nh = false; nv = false; nd = true;  break;
    case 3: nh = false; nv = false; nd = false; break;
    case 4: nh = true;  nv = true;  nd = true;  break;
    case 5: nh = true;  nv = true;  nd = false; break;
    case 6: nh = false; nv = true;  nd = true;  break;
    default: nh = false; nv = true; nd = false; break;
    }
    dodoe::UInt32 result = id;
    if (nh) result |= dodoe::kTileFlipHorizontal;
    if (nv) result |= dodoe::kTileFlipVertical;
    if (nd) result |= dodoe::kTileFlipDiagonal;
    return result;
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

void TilePaintService::setTool(TileTool t) {
    m_tool = t;
    m_hasAnchor = false;
    clearSelection();
}

void TilePaintService::flipBrushX() {
    TileBrush next = m_brush;
    next.gids.assign(m_brush.gids.size(), 0);
    for (int y = 0; y < m_brush.h; ++y) {
        for (int x = 0; x < m_brush.w; ++x) {
            const dodoe::UInt32 gid =
                m_brush.gids[static_cast<std::size_t>(y * m_brush.w + (m_brush.w - 1 - x))];
            next.gids[static_cast<std::size_t>(y * m_brush.w + x)] =
                gid ^ dodoe::kTileFlipHorizontal;
        }
    }
    m_brush = std::move(next);
}

void TilePaintService::flipBrushY() {
    TileBrush next = m_brush;
    next.gids.assign(m_brush.gids.size(), 0);
    for (int y = 0; y < m_brush.h; ++y) {
        for (int x = 0; x < m_brush.w; ++x) {
            const dodoe::UInt32 gid =
                m_brush.gids[static_cast<std::size_t>((m_brush.h - 1 - y) * m_brush.w + x)];
            next.gids[static_cast<std::size_t>(y * m_brush.w + x)] =
                gid ^ dodoe::kTileFlipVertical;
        }
    }
    m_brush = std::move(next);
}

void TilePaintService::rotateBrushCW() {
    TileBrush next;
    next.w = m_brush.h;
    next.h = m_brush.w;
    next.gids.assign(m_brush.gids.size(), 0);
    for (int y = 0; y < m_brush.h; ++y) {
        for (int x = 0; x < m_brush.w; ++x) {
            const dodoe::UInt32 gid =
                m_brush.gids[static_cast<std::size_t>(y * m_brush.w + x)];
            const int nx = m_brush.h - 1 - y;
            const int ny = x;
            next.gids[static_cast<std::size_t>(ny * next.w + nx)] = RotateGidCW(gid);
        }
    }
    m_brush = std::move(next);
}

void TilePaintService::onCellDown(int cx, int cy) {
    if (!hasTarget()) return;

    if (m_tool == TileTool::Select) {
        if (m_hasSelection && cx >= m_selX && cx < m_selX + m_selW &&
            cy >= m_selY && cy < m_selY + m_selH) {
            m_moving = true;
            m_selecting = false;
            m_moveSnapshot = snapshotSelection();
        } else {
            m_moving = false;
            m_selecting = true;
            m_hasSelection = false;
            m_moveSnapshot.clear();
        }
        m_anchorX = m_lastX = cx;
        m_anchorY = m_lastY = cy;
        m_hasAnchor = true;
        return;
    }

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        m_anchorX = m_lastX = cx;
        m_anchorY = m_lastY = cy;
        m_hasAnchor = true;
        return;
    }

    m_session.history().beginMerge();

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(cmd.get(), m_layer, cx, cy, m_brush, m_randomBrush);
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
    if (!hasTarget()) return;

    if (m_tool == TileTool::Select || m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        m_lastX = cx;
        m_lastY = cy;
        return;
    }

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(cmd.get(), m_layer, cx, cy, m_brush, m_randomBrush);
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
    if (!hasTarget()) return;

    if (m_tool == TileTool::Select) {
        if (m_moving) {
            const int dx = m_lastX - m_anchorX;
            const int dy = m_lastY - m_anchorY;
            if (dx != 0 || dy != 0) {
                applySelectionMove(dx, dy);
            }
        } else if (m_selecting) {
            const int x0 = std::min(m_anchorX, m_lastX);
            const int y0 = std::min(m_anchorY, m_lastY);
            const int w = std::abs(m_lastX - m_anchorX) + 1;
            const int h = std::abs(m_lastY - m_anchorY) + 1;
            m_selX = x0;
            m_selY = y0;
            m_selW = w;
            m_selH = h;
            m_hasSelection = true;
        }
        m_moving = false;
        m_selecting = false;
        m_hasAnchor = false;
        m_moveSnapshot.clear();
        return;
    }

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        if (m_hasAnchor) {
            auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);
            if (m_tool == TileTool::Line) {
                TraceLine(cmd.get(), m_layer, m_anchorX, m_anchorY, m_lastX, m_lastY, m_brush,
                          m_randomBrush);
            } else {
                int x0 = m_anchorX < m_lastX ? m_anchorX : m_lastX;
                int x1 = m_anchorX > m_lastX ? m_anchorX : m_lastX;
                int y0 = m_anchorY < m_lastY ? m_anchorY : m_lastY;
                int y1 = m_anchorY > m_lastY ? m_anchorY : m_lastY;
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        applyBrush(cmd.get(), m_layer, x, y, m_brush, m_randomBrush);
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

std::vector<dodoe::UInt32> TilePaintService::snapshotSelection() const {
    std::vector<dodoe::UInt32> snapshot;
    if (!m_hasSelection || m_selW <= 0 || m_selH <= 0) {
        return snapshot;
    }
    snapshot.resize(static_cast<std::size_t>(m_selW) * m_selH, 0);
    for (int y = 0; y < m_selH; ++y) {
        for (int x = 0; x < m_selW; ++x) {
            snapshot[static_cast<std::size_t>(y * m_selW + x)] = readTile(m_layer, m_selX + x, m_selY + y);
        }
    }
    return snapshot;
}

void TilePaintService::applySelectionMove(int dx, int dy) {
    if (!m_hasSelection || m_moveSnapshot.size() != static_cast<std::size_t>(m_selW) * m_selH) {
        return;
    }
    const dodoe::TileLayerComponent* layer = ActiveLayer(m_layer);
    if (!layer) return;

    std::map<std::pair<int, int>, dodoe::UInt32> finalCells;
    for (int y = 0; y < m_selH; ++y) {
        for (int x = 0; x < m_selW; ++x) {
            finalCells[{m_selX + x, m_selY + y}] = 0;
        }
    }
    for (int y = 0; y < m_selH; ++y) {
        for (int x = 0; x < m_selW; ++x) {
            const int tx = m_selX + x + dx;
            const int ty = m_selY + y + dy;
            if (!CellInLayer(*layer, tx, ty)) continue;
            finalCells[{tx, ty}] =
                m_moveSnapshot[static_cast<std::size_t>(y * m_selW + x)];
        }
    }

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);
    for (const auto& [cell, after] : finalCells) {
        const dodoe::UInt32 before = readTile(m_layer, cell.first, cell.second);
        if (before != after) {
            cmd->addCell(cell.first, cell.second, before, after);
        }
    }
    if (!cmd->empty()) {
        m_session.history().execute(std::move(cmd), m_session.documentModel());
        m_session.notifyDocumentChanged();
    }
    m_selX += dx;
    m_selY += dy;
}

void TilePaintService::deleteSelection() {
    if (!hasTarget() || !m_hasSelection) return;
    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);
    for (int y = 0; y < m_selH; ++y) {
        for (int x = 0; x < m_selW; ++x) {
            const int gx = m_selX + x;
            const int gy = m_selY + y;
            const dodoe::UInt32 before = readTile(m_layer, gx, gy);
            if (before != 0) {
                cmd->addCell(gx, gy, before, 0);
            }
        }
    }
    if (!cmd->empty()) {
        m_session.history().execute(std::move(cmd), m_session.documentModel());
        m_session.notifyDocumentChanged();
    }
}

void TilePaintService::copySelection() {
    if (!m_hasSelection) return;
    m_clipboard.w = m_selW;
    m_clipboard.h = m_selH;
    m_clipboard.gids = snapshotSelection();
}

void TilePaintService::pasteClipboard() {
    if (m_clipboard.gids.empty()) return;
    setTool(TileTool::Brush);
    m_brush = m_clipboard;
}

void TilePaintService::clearSelection() {
    m_hasSelection = false;
    m_selecting = false;
    m_moving = false;
    m_moveSnapshot.clear();
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
