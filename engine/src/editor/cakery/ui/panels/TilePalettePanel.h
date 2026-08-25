// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>
#include <functional>
#include <vector>

#include <nlohmann/json.hpp>

class QLabel;
class QScrollArea;
class QToolButton;
class QVBoxLayout;
class QPixmap;
class QMouseEvent;
class QPaintEvent;

namespace cakery {

class EditorWorkspaceContext;

class TileTilesetView final : public QWidget {
public:
    TileTilesetView(QPixmap image, std::uint32_t tileWidth, std::uint32_t tileHeight,
                    std::uint32_t columns, std::uint32_t firstGid, QWidget* parent = nullptr);

    void setSelection(int cellX, int cellY, int cellW, int cellH);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    [[nodiscard]] std::uint32_t firstGid() const { return m_firstGid; }
    [[nodiscard]] std::uint32_t columns() const { return m_columns; }
    [[nodiscard]] std::uint32_t tileCount() const {
        if (m_columns == 0) return 0;
        return static_cast<std::uint32_t>(m_image.height()) / m_tileHeight * m_columns;
    }

    std::function<void(int w, int h, std::vector<std::uint32_t> gids)> onBrushSelected;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect selectionRect() const;
    void emitBrush();

    QPixmap m_image;
    std::uint32_t m_tileWidth;
    std::uint32_t m_tileHeight;
    std::uint32_t m_columns;
    std::uint32_t m_firstGid;
    QPoint m_selectStart;
    QPoint m_selectEnd;
    QPoint m_selectionCell;
    int m_selectionW = 1;
    int m_selectionH = 1;
    bool m_selecting = false;
};

class TilePalettePanel final : public QWidget {
    Q_OBJECT
public:
    explicit TilePalettePanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~TilePalettePanel() override;

    void refresh();
    void onNewTilemap();

private:
    void onAddTileset();
    void onRemoveTileset();
    void applyBrushHighlight();
    std::string resolveImagePath(const std::string& url) const;
    void rebuildTilesets();

    EditorWorkspaceContext& m_context;
    QScrollArea* m_scroll = nullptr;
    QVBoxLayout* m_tilesetsLayout = nullptr;
    QLabel* m_statusLabel = nullptr;
    QToolButton* m_newTilemapButton = nullptr;
    QToolButton* m_addTilesetButton = nullptr;
    std::vector<TileTilesetView*> m_tilesetViews;
    nlohmann::json m_state;
    ScopedConnection m_selectionConnection;
    ScopedConnection m_documentConnection;
    ScopedConnection m_tileModeConnection;
};

} // namespace cakery
