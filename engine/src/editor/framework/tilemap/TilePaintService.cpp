// do@Redlive

#include "TilePaintService.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/PaintTilesCommand.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"

namespace cakery {

static UInt32 readTile(EditorContext& ctx, dodoe::UUID layerEntity, int x, int y) {
    auto* scene = ctx.activeScene();
    if (!scene) return 0;
    auto entity = ResolveEntity(scene, layerEntity);
    if (!entity.valid()) return 0;
    auto* layer = entity.hasComponent<dodoe::TileLayerComponent>()
                      ? &entity.getComponent<dodoe::TileLayerComponent>()
                      : nullptr;
    if (!layer) return 0;
    return layer->getTile(x, y);
}

static void applyBrush(EditorContext& ctx, PaintTilesCommand* cmd,
                        dodoe::UUID layerEntity, int cx, int cy, const TileBrush& brush) {
    for (int by = 0; by < brush.h; ++by) {
        for (int bx = 0; bx < brush.w; ++bx) {
            int gx = cx + bx;
            int gy = cy + by;
            UInt32 gid = brush.gids[by * brush.w + bx];
            UInt32 before = readTile(ctx, layerEntity, gx, gy);
            if (before != gid) {
                cmd->addCell(gx, gy, before, gid);
            }
        }
    }
}

// Bresenham 直线：从 (x0,y0) 到 (x1,y1) 逐格应用 brush
static void TraceLine(EditorContext& ctx, PaintTilesCommand* cmd,
                      dodoe::UUID layerEntity, int x0, int y0, int x1, int y1,
                      const TileBrush& brush) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        applyBrush(ctx, cmd, layerEntity, x0, y0, brush);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void TilePaintService::setActiveEntity(dodoe::UUID entity) {
    auto* scene = m_ctx.activeScene();
    if (!scene) return;
    auto e = ResolveEntity(scene, entity);
    if (!e.valid()) return;

    if (e.hasComponent<dodoe::TilemapComponent>()) {
        setActiveTilemap(entity);
        // 尚无活动层时，自动选中该 tilemap 下的第一个图层
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
        // 活动 tilemap 缺失时，从层的父级推导
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
    if (!hasTarget()) return;

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        // 按下仅记录锚点，抬起时一次性提交
        m_anchorX = m_lastX = cx;
        m_anchorY = m_lastY = cy;
        m_hasAnchor = true;
        return;
    }

    m_ctx.commands().beginMerge();

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(m_ctx, cmd.get(), m_layer, cx, cy, m_brush);
    } else if (m_tool == TileTool::Erase) {
        TileBrush eraser;
        eraser.w = m_brush.w;
        eraser.h = m_brush.h;
        eraser.gids.assign(eraser.w * eraser.h, 0);
        applyBrush(m_ctx, cmd.get(), m_layer, cx, cy, eraser);
    } else if (m_tool == TileTool::Picker) {
        UInt32 gid = readTile(m_ctx, m_layer, cx, cy);
        if (gid > 0) {
            m_brush.w = 1;
            m_brush.h = 1;
            m_brush.gids = {gid};
        }
        m_ctx.commands().endMerge();
        return;
    } else if (m_tool == TileTool::Fill) {
        UInt32 targetGid = readTile(m_ctx, m_layer, cx, cy);
        UInt32 fillGid = m_brush.gids[0];
        if (fillGid == targetGid) {
            m_ctx.commands().endMerge();
            return;
        }
        auto* scene = m_ctx.activeScene();
        if (!scene) { m_ctx.commands().endMerge(); return; }
        auto entity = ResolveEntity(scene, m_layer);
        if (!entity.valid()) { m_ctx.commands().endMerge(); return; }
        auto* layer = entity.hasComponent<dodoe::TileLayerComponent>()
                          ? &entity.getComponent<dodoe::TileLayerComponent>()
                          : nullptr;
        if (!layer) { m_ctx.commands().endMerge(); return; }
        const Int32 w = static_cast<Int32>(layer->layer_width);
        const Int32 h = static_cast<Int32>(layer->layer_height);
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) { m_ctx.commands().endMerge(); return; }

        // BFS 四连通洪泛：只替换与种子同色的连续区域
        struct CellPos { Int32 x, y; };
        dodoe::DynamicArray<UInt8> visited(static_cast<dodoe::Size_t>(w) * h, 0);
        dodoe::DynamicArray<CellPos> queue;
        queue.push_back({cx, cy});
        visited[static_cast<dodoe::Size_t>(cy) * w + cx] = 1;

        const Int32 dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!queue.empty()) {
            CellPos cur = queue.back();
            queue.pop_back();
            if (layer->getTile(cur.x, cur.y) != targetGid) continue;
            cmd->addCell(cur.x, cur.y, targetGid, fillGid);
            for (const auto& d : dirs) {
                Int32 nx = cur.x + d[0];
                Int32 ny = cur.y + d[1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                if (visited[static_cast<dodoe::Size_t>(ny) * w + nx]) continue;
                visited[static_cast<dodoe::Size_t>(ny) * w + nx] = 1;
                if (layer->getTile(nx, ny) == targetGid) {
                    queue.push_back({nx, ny});
                }
            }
        }
    }

    if (!cmd->empty()) {
        m_ctx.commands().execute(std::move(cmd));
    } else {
        m_ctx.commands().endMerge();
    }
}

void TilePaintService::onCellDrag(int cx, int cy) {
    if (!hasTarget()) return;

    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        m_lastX = cx;
        m_lastY = cy;
        return;
    }

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush) {
        applyBrush(m_ctx, cmd.get(), m_layer, cx, cy, m_brush);
    } else if (m_tool == TileTool::Erase) {
        TileBrush eraser;
        eraser.w = m_brush.w;
        eraser.h = m_brush.h;
        eraser.gids.assign(eraser.w * eraser.h, 0);
        applyBrush(m_ctx, cmd.get(), m_layer, cx, cy, eraser);
    }

    if (!cmd->empty()) {
        m_ctx.commands().execute(std::move(cmd));
    }
}

void TilePaintService::onCellUp() {
    if (m_tool == TileTool::Line || m_tool == TileTool::Rect) {
        if (m_hasAnchor) {
            auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);
            if (m_tool == TileTool::Line) {
                TraceLine(m_ctx, cmd.get(), m_layer,
                          m_anchorX, m_anchorY, m_lastX, m_lastY, m_brush);
            } else {
                int x0 = m_anchorX < m_lastX ? m_anchorX : m_lastX;
                int x1 = m_anchorX > m_lastX ? m_anchorX : m_lastX;
                int y0 = m_anchorY < m_lastY ? m_anchorY : m_lastY;
                int y1 = m_anchorY > m_lastY ? m_anchorY : m_lastY;
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        applyBrush(m_ctx, cmd.get(), m_layer, x, y, m_brush);
                    }
                }
            }
            if (!cmd->empty()) {
                m_ctx.commands().execute(std::move(cmd));
            }
        }
        m_hasAnchor = false;
        return;
    }

    m_ctx.commands().endMerge();
}

} // namespace cakery
