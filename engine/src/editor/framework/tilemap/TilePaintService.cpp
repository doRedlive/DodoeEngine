// do@Redlive

#include "TilePaintService.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/PaintTilesCommand.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"

namespace cakery {

static UInt32 readTile(EditorContext& ctx, dodoe::Uuid layerEntity, int x, int y) {
    auto* scene = ctx.activeScene();
    if (!scene) return 0;
    auto entity = ResolveEntity(scene, layerEntity);
    if (!entity.valid()) return 0;
    auto* layer = entity.tryGetComponent<dodoe::TileLayerComponent>();
    if (!layer) return 0;
    return layer->getTile(x, y);
}

static void applyBrush(EditorContext& ctx, PaintTilesCommand* cmd,
                        dodoe::Uuid layerEntity, int cx, int cy, const TileBrush& brush) {
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

void TilePaintService::onCellDown(int cx, int cy) {
    if (!hasTarget()) return;

    m_ctx.commands().beginMerge();

    auto cmd = std::make_unique<PaintTilesCommand>(m_tilemap, m_layer);

    if (m_tool == TileTool::Brush || m_tool == TileTool::Rect) {
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
        if (m_brush.gids.empty() || m_brush.gids[0] == targetGid) {
            m_ctx.commands().endMerge();
            return;
        }
        UInt32 fillGid = m_brush.gids[0];
        auto* scene = m_ctx.activeScene();
        if (!scene) { m_ctx.commands().endMerge(); return; }
        auto entity = ResolveEntity(scene, m_layer);
        if (!entity.valid()) { m_ctx.commands().endMerge(); return; }
        auto* layer = entity.tryGetComponent<dodoe::TileLayerComponent>();
        if (!layer) { m_ctx.commands().endMerge(); return; }
        for (UInt32 y = 0; y < layer->layer_height; ++y) {
            for (UInt32 x = 0; x < layer->layer_width; ++x) {
                if (layer->getTile(static_cast<Int32>(x), static_cast<Int32>(y)) == targetGid) {
                    cmd->addCell(static_cast<Int32>(x), static_cast<Int32>(y), targetGid, fillGid);
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
    m_ctx.commands().endMerge();
}

} // namespace cakery
