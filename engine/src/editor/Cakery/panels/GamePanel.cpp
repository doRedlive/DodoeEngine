// do@Redlive

#include "GamePanel.h"
#include "framework/EditorContext.h"

#include <QPaintEvent>
#include <QPainter>

namespace cakery {

GamePanel::GamePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(320, 240);
}

void GamePanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(rect(), Qt::AlignCenter, "No Cameras Rendering");
}

} // namespace cakery
