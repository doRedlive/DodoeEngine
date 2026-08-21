#pragma once

#include "Panel.h"
#include "framework/tilemap/TilePaintService.h"
#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QLabel>

class QActionGroup;
class QListWidget;

namespace cakery {

class TilesetPreview;

class TilePalettePanel : public Panel {
    Q_OBJECT
public:
    explicit TilePalettePanel(EditorContext& ctx, QWidget* parent = nullptr);

private:
    void buildToolbar();
    void buildLayerList();
    void refreshAll();
    void updateLayerList();
    void updatePaletteView();

    void onCreateTilemap();
    void onCreateLayer();

    QToolBar* m_toolbar = nullptr;
    QActionGroup* m_toolGroup = nullptr;
    QWidget* m_paletteView = nullptr;
    TilesetPreview* m_preview = nullptr;
    QLabel* m_infoLabel = nullptr;
    QListWidget* m_layerList = nullptr;
};

} // namespace cakery
