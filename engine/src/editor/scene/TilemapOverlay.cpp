// do@Redlive
#include "dopch.h"
#include "TilemapOverlay.h"
#include "SceneOverlayGeometry.h"
#include "services/TilePaintService.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include <algorithm>
#include <cmath>

using namespace dodoe;
namespace cakery {
using namespace scene_overlay;
void drawTilemapOverlay(GizmoChannelData& data, const TilePaintService& paint, Entity tilemap)
{
    const auto& comp = tilemap.getComponent<TilemapComponent>();
    const float mapW = static_cast<float>(comp.map_width * comp.tile_width);
    const float mapH = static_cast<float>(comp.map_height * comp.tile_height);

    const Color borderColor{1.0f, 1.0f, 1.0f, 0.9f};
    const Color gridColor{1.0f, 1.0f, 1.0f, 0.10f};
    const Color ghostColor{0.25f, 1.0f, 0.35f, 1.0f};
    const Color selectColor{1.0f, 0.82f, 0.3f, 1.0f};


    auto addCellRect = [&data, &comp](int cellX, int cellY, int cellsW, int cellsH,
                                      const Color& color) {
        const float x0 = static_cast<float>(cellX * comp.tile_width);
        const float y0 = static_cast<float>(cellY * comp.tile_height);
        const float x1 = x0 + static_cast<float>(cellsW * comp.tile_width);
        const float y1 = y0 + static_cast<float>(cellsH * comp.tile_height);
        AddLine(data, Vector3f(x0, y0, 0.0f), Vector3f(x1, y0, 0.0f), color);
        AddLine(data, Vector3f(x1, y0, 0.0f), Vector3f(x1, y1, 0.0f), color);
        AddLine(data, Vector3f(x1, y1, 0.0f), Vector3f(x0, y1, 0.0f), color);
        AddLine(data, Vector3f(x0, y1, 0.0f), Vector3f(x0, y0, 0.0f), color);
    };

    AddLine(data, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(mapW, 0.0f, 0.0f), borderColor);
    AddLine(data, Vector3f(mapW, 0.0f, 0.0f), Vector3f(mapW, mapH, 0.0f), borderColor);
    AddLine(data, Vector3f(mapW, mapH, 0.0f), Vector3f(0.0f, mapH, 0.0f), borderColor);
    AddLine(data, Vector3f(0.0f, mapH, 0.0f), Vector3f(0.0f, 0.0f, 0.0f), borderColor);

    const int gw = static_cast<int>(comp.map_width);
    const int gh = static_cast<int>(comp.map_height);
    if (gw <= 256 && gh <= 256) {
        for (int x = 1; x < gw; ++x) {
            const float px = static_cast<float>(x * comp.tile_width);
            AddLine(data, Vector3f(px, 0.0f, 0.0f), Vector3f(px, mapH, 0.0f), gridColor);
        }
        for (int y = 1; y < gh; ++y) {
            const float py = static_cast<float>(y * comp.tile_height);
            AddLine(data, Vector3f(0.0f, py, 0.0f), Vector3f(mapW, py, 0.0f), gridColor);
        }
    }

    const TileTool tool = paint.tool();
    if (tool == TileTool::Select) {
        if (paint.isMoving() && paint.hasAnchor()) {
            addCellRect(paint.selectionX(), paint.selectionY(),
                        paint.selectionW(), paint.selectionH(), gridColor);
            addCellRect(paint.selectionX() + paint.moveDeltaX(),
                        paint.selectionY() + paint.moveDeltaY(),
                        paint.selectionW(), paint.selectionH(), selectColor);
        } else if (paint.isSelecting() && paint.hasAnchor()) {
            const int x0 = std::min(paint.anchorX(), paint.lastX());
            const int y0 = std::min(paint.anchorY(), paint.lastY());
            const int w = std::abs(paint.lastX() - paint.anchorX()) + 1;
            const int h = std::abs(paint.lastY() - paint.anchorY()) + 1;
            addCellRect(x0, y0, w, h, selectColor);
        } else if (paint.hasSelection()) {
            addCellRect(paint.selectionX(), paint.selectionY(),
                        paint.selectionW(), paint.selectionH(), selectColor);
        }
        data.has_data = true;
        return;
    }

    const TileBrush& brush = paint.brush();
    if (paint.hasAnchor() && (tool == TileTool::Line || tool == TileTool::Rect)) {
        const int ax = paint.anchorX();
        const int ay = paint.anchorY();
        const int lx = paint.lastX();
        const int ly = paint.lastY();
        if (tool == TileTool::Rect) {
            const int x0 = std::min(ax, lx);
            const int y0 = std::min(ay, ly);
            const int w = std::abs(lx - ax) + brush.w;
            const int h = std::abs(ly - ay) + brush.h;
            addCellRect(x0, y0, w, h, ghostColor);
        } else {
            int x = ax;
            int y = ay;
            const int dx = lx > ax ? lx - ax : ax - lx;
            const int dy = ly > ay ? ly - ay : ay - ly;
            const int sx = ax < lx ? 1 : -1;
            const int sy = ay < ly ? 1 : -1;
            int err = dx - dy;
            for (;;) {
                addCellRect(x, y, brush.w, brush.h, ghostColor);
                if (x == lx && y == ly) break;
                const int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x += sx; }
                if (e2 < dx)  { err += dx; y += sy; }
            }
        }
    } else if (paint.hasHover()) {
        addCellRect(paint.hoverX(), paint.hoverY(), brush.w, brush.h, ghostColor);
    }

    data.has_data = true;
}
} // namespace cakery
