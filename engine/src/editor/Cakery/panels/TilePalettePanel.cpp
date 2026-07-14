#include "TilePalettePanel.h"
#include "framework/EditorContext.h"

#include <QVBoxLayout>

namespace cakery {

TilePalettePanel::TilePalettePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildToolbar();

    m_paletteView = new QWidget(this);
    m_paletteView->setMinimumSize(200, 200);
    m_paletteView->setStyleSheet("background-color: #2a2a2a;");
    layout->addWidget(m_paletteView, 1);

    m_infoLabel = new QLabel("Select a tilemap to edit", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #888; font-size: 12px; padding: 4px;");
    layout->addWidget(m_infoLabel);
}

void TilePalettePanel::buildToolbar()
{
    m_toolbar = new QToolBar("Tile Tools", this);

    auto* brushAction = m_toolbar->addAction("Brush");
    brushAction->setCheckable(true);
    brushAction->setChecked(true);

    auto* eraseAction = m_toolbar->addAction("Erase");
    eraseAction->setCheckable(true);

    auto* fillAction = m_toolbar->addAction("Fill");
    fillAction->setCheckable(true);

    auto* rectAction = m_toolbar->addAction("Rect");
    rectAction->setCheckable(true);

    auto* pickerAction = m_toolbar->addAction("Picker");
    pickerAction->setCheckable(true);

    connect(brushAction, &QAction::triggered, [this]() { m_currentTool = TileTool::Brush; });
    connect(eraseAction, &QAction::triggered, [this]() { m_currentTool = TileTool::Erase; });
    connect(fillAction,  &QAction::triggered, [this]() { m_currentTool = TileTool::Fill; });
    connect(rectAction,  &QAction::triggered, [this]() { m_currentTool = TileTool::Rect; });
    connect(pickerAction,&QAction::triggered, [this]() { m_currentTool = TileTool::Picker; });

    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        layout->insertWidget(0, m_toolbar);
    }
}

void TilePalettePanel::updatePaletteView()
{
}

} // namespace cakery
