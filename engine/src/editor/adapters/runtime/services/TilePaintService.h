// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"

#include <utility>
#include <vector>

namespace cakery {

class EditorSession;

enum class TileTool { Select, Brush, Erase, Fill, Rect, Picker, Line };

struct TileBrush {
    int w = 1, h = 1;
    std::vector<dodoe::UInt32> gids{0};
    bool empty() const { for (auto g : gids) if (g) return false; return true; }
};

class TilePaintService {
public:
    explicit TilePaintService(EditorSession& session) : m_session(session) {}

    void setActiveTilemap(dodoe::UUID tilemapEntity) { m_tilemap = tilemapEntity; }
    void setActiveLayer(dodoe::UUID layerEntity)     { m_layer = layerEntity; }
    dodoe::UUID activeTilemap() const { return m_tilemap; }
    dodoe::UUID activeLayer()  const { return m_layer; }

    void setActiveEntity(dodoe::UUID entity);

    static void RegisterCommands();

    void setTool(TileTool t);
    TileTool tool() const          { return m_tool; }
    void setBrush(TileBrush b)     { m_brush = std::move(b); }
    const TileBrush& brush() const { return m_brush; }

    void flipBrushX();
    void flipBrushY();
    void rotateBrushCW();

    void setRandomBrush(bool enabled) { m_randomBrush = enabled; }
    bool randomBrush() const          { return m_randomBrush; }

    void onCellDown(int cx, int cy);
    void onCellDrag(int cx, int cy);
    void onCellUp();

    void deleteSelection();
    void copySelection();
    void pasteClipboard();
    void clearSelection();

    [[nodiscard]] bool hasSelection() const { return m_hasSelection; }
    [[nodiscard]] int selectionX() const { return m_selX; }
    [[nodiscard]] int selectionY() const { return m_selY; }
    [[nodiscard]] int selectionW() const { return m_selW; }
    [[nodiscard]] int selectionH() const { return m_selH; }
    [[nodiscard]] bool isSelecting() const { return m_selecting; }
    [[nodiscard]] bool isMoving() const { return m_moving; }
    [[nodiscard]] int moveDeltaX() const { return m_hasAnchor ? m_lastX - m_anchorX : 0; }
    [[nodiscard]] int moveDeltaY() const { return m_hasAnchor ? m_lastY - m_anchorY : 0; }

    void setHoverCell(int cx, int cy) { m_hoverX = cx; m_hoverY = cy; m_hasHover = true; }
    void clearHover() { m_hasHover = false; }
    [[nodiscard]] bool hasHover() const { return m_hasHover; }
    [[nodiscard]] int hoverX() const { return m_hoverX; }
    [[nodiscard]] int hoverY() const { return m_hoverY; }

    [[nodiscard]] bool hasAnchor() const { return m_hasAnchor; }
    [[nodiscard]] int anchorX() const { return m_anchorX; }
    [[nodiscard]] int anchorY() const { return m_anchorY; }
    [[nodiscard]] int lastX() const { return m_lastX; }
    [[nodiscard]] int lastY() const { return m_lastY; }

    bool hasTarget() const { return m_tilemap.isValid() && m_layer.isValid(); }

private:
    void applySelectionMove(int dx, int dy);
    std::vector<dodoe::UInt32> snapshotSelection() const;

    EditorSession& m_session;
    dodoe::UUID m_tilemap;
    dodoe::UUID m_layer;
    TileTool  m_tool = TileTool::Select;
    TileBrush m_brush;
    TileBrush m_clipboard;
    bool m_randomBrush = false;

    int  m_anchorX{0}, m_anchorY{0};
    int  m_lastX{0}, m_lastY{0};
    int  m_hoverX{0}, m_hoverY{0};
    bool m_hasAnchor{false};
    bool m_hasHover{false};

    bool m_hasSelection{false};
    bool m_selecting{false};
    bool m_moving{false};
    int  m_selX{0}, m_selY{0}, m_selW{0}, m_selH{0};
    std::vector<dodoe::UInt32> m_moveSnapshot;
};

} // namespace cakery
