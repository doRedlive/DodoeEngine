// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>
#include <functional>
#include <vector>


class QLabel;
class QActionGroup;
class QComboBox;
class QScrollArea;
class QToolBar;
class QToolButton;
class QVBoxLayout;
class QPixmap;
class QMouseEvent;
class QPaintEvent;

namespace cakery {

class EditorWorkspaceContext;
class TilePaletteViewModel;

class TileTilesetView final : public QWidget {
public:
    TileTilesetView(QPixmap image, UInt32 tileWidth, UInt32 tileHeight,
                    UInt32 columns, UInt32 firstGid,
                    UInt32 margin = 0, UInt32 spacing = 0,
                    QWidget* parent = nullptr);

    void setSelection(int cellX, int cellY, int cellW, int cellH);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    [[nodiscard]] UInt32 firstGid() const { return m_firstGid; }
    [[nodiscard]] UInt32 columns() const { return m_columns; }
    [[nodiscard]] UInt32 tileCount() const {
        if (m_columns == 0 || m_tileHeight + m_spacing == 0) return 0;
        const UInt32 stepH = m_tileHeight + m_spacing;
        const UInt32 rows = static_cast<UInt32>(m_image.height()) >= m_margin * 2
            ? (static_cast<UInt32>(m_image.height()) - m_margin * 2 + stepH - 1) / stepH
            : 0;
        return rows * m_columns;
    }

    std::function<void(int w, int h, DynamicArray<UInt32> gids)> onBrushSelected;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect selectionRect() const;
    int cellAtX(const QPoint& pos) const;
    int cellAtY(const QPoint& pos) const;
    void emitBrush();

    QPixmap m_image;
    UInt32 m_tileWidth;
    UInt32 m_tileHeight;
    UInt32 m_columns;
    UInt32 m_firstGid;
    UInt32 m_margin;
    UInt32 m_spacing;
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
    void onViewModelChanged();
    void onAddTileset();
    void onRemoveTileset();
    void onEditTileset();
    void onResizeTilemap();
    void applyBrushHighlight();
    void rebuildTargetOptions(UInt64 activeUuid);
    void updateToolState(bool active);
    void setActiveTarget(int index);
    void rebuildTilesets();

    EditorWorkspaceContext& m_context;
    Scope<TilePaletteViewModel> m_viewModel;
    QToolBar* m_toolBar = nullptr;
    QActionGroup* m_toolGroup = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QLabel* m_brushLabel = nullptr;
    QScrollArea* m_scroll = nullptr;
    QVBoxLayout* m_tilesetsLayout = nullptr;
    QLabel* m_statusLabel = nullptr;
    QToolButton* m_newTilemapButton = nullptr;
    QToolButton* m_addTilesetButton = nullptr;
    QToolButton* m_removeTilesetButton = nullptr;
    QToolButton* m_editTilesetButton = nullptr;
    QToolButton* m_resizeTilemapButton = nullptr;
    QToolButton* m_randomBrushButton = nullptr;
    DynamicArray<TileTilesetView*> m_tilesetViews;
    UInt64 m_activeTarget = 0;
};

} // namespace cakery
