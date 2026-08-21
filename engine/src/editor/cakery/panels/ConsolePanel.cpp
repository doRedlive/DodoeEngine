// do@Redlive

#include "ConsolePanel.h"
#include "framework/EditorContext.h"

#include <QVBoxLayout>

namespace cakery {

ConsolePanel::ConsolePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    auto* list = new QListWidget(this);
    layout->addWidget(list);
}

} // namespace cakery
