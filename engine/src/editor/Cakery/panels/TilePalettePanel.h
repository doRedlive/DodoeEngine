#pragma once

#include "Panel.h"
#include "framework/tilemap/TilePaintService.h"
#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QLabel>

namespace cakery {

class TilePalettePanel : public Panel {
    Q_OBJECT
public:
    explicit TilePalettePanel(EditorContext& ctx, QWidget* parent = nullptr);

private:
    void buildToolbar();
    void updatePaletteView();

    QToolBar* m_toolbar = nullptr;
    QWidget* m_paletteView = nullptr;
    QLabel* m_infoLabel = nullptr;

    TileTool m_currentTool = TileTool::Brush;
};

} // namespace cakery
