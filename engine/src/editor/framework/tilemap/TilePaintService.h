// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"
#include <vector>

namespace cakery {

class EditorContext;

enum class TileTool { Brush, Erase, Fill, Rect, Picker, Line };

struct TileBrush {
    int w = 1, h = 1;
    std::vector<dodoe::UInt32> gids{0};
    bool empty() const { for (auto g : gids) if (g) return false; return true; }
};

class TilePaintService {
public:
    explicit TilePaintService(EditorContext& ctx) : m_ctx(ctx) {}

    void setActiveTilemap(dodoe::UUID tilemapEntity) { m_tilemap = tilemapEntity; }
    void setActiveLayer(dodoe::UUID layerEntity)     { m_layer = layerEntity; }
    dodoe::UUID activeTilemap() const { return m_tilemap; }
    dodoe::UUID activeLayer()  const { return m_layer; }

    // 根据实体组件自动激活目标：含 TilemapComponent 设为活动 tilemap，含 TileLayerComponent 设为活动层
    void setActiveEntity(dodoe::UUID entity);

    void setTool(TileTool t)       { m_tool = t; }
    TileTool tool() const          { return m_tool; }
    void setBrush(TileBrush b)     { m_brush = std::move(b); }
    const TileBrush& brush() const { return m_brush; }

    void onCellDown(int cx, int cy);
    void onCellDrag(int cx, int cy);
    void onCellUp();

    bool hasTarget() const { return m_tilemap.isValid() && m_layer.isValid(); }

private:
    EditorContext& m_ctx;
    dodoe::UUID m_tilemap;
    dodoe::UUID m_layer;
    TileTool  m_tool = TileTool::Brush;
    TileBrush m_brush;

    int  m_anchorX{0}, m_anchorY{0};
    int  m_lastX{0}, m_lastY{0};
    bool m_hasAnchor{false};
};

} // namespace cakery
