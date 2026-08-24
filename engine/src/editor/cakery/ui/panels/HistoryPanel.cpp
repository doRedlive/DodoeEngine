// do@Redlive

#include "HistoryPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "core/history/EditorHistory.h"

#include <QAbstractItemView>
#include <QColor>
#include <QListWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <iterator>
#include <string>

namespace cakery {

HistoryPanel::HistoryPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, &HistoryPanel::onItemClicked);
    m_historySubscription = m_context.session().history().subscribe([this]() { refresh(); });
    refresh();
}

void HistoryPanel::refresh()
{
    const EditorHistory& history = m_context.session().history();
    const auto& undoStack = history.undoStack();
    const auto& redoStack = history.redoStack();
    m_list->clear();

    for (auto it = redoStack.rbegin(); it != redoStack.rend(); ++it) {
        auto* item = new QListWidgetItem(QString::fromStdString((*it)->label()), m_list);
        item->setForeground(QColor("#6f6f6f"));
        item->setData(Qt::UserRole, QStringLiteral("redo"));
        item->setData(Qt::UserRole + 1, static_cast<int>(std::distance(redoStack.rbegin(), it) + 1));
    }
    if (!redoStack.empty()) {
        auto* separator = new QListWidgetItem(QStringLiteral("────"), m_list);
        separator->setFlags(Qt::NoItemFlags);
        separator->setForeground(QColor("#4f4f4f"));
    }
    for (auto it = undoStack.rbegin(); it != undoStack.rend(); ++it) {
        auto* item = new QListWidgetItem(QString::fromStdString((*it)->label()), m_list);
        item->setData(Qt::UserRole, QStringLiteral("undo"));
        item->setData(Qt::UserRole + 1, static_cast<int>(std::distance(undoStack.rbegin(), it) + 1));
    }
}

void HistoryPanel::onItemClicked(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    const QString kind = item->data(Qt::UserRole).toString();
    const int steps = item->data(Qt::UserRole + 1).toInt();
    QTimer::singleShot(0, this, [this, kind, steps]() {
        if (kind == QStringLiteral("undo")) {
            for (int i = 0; i < steps; ++i) {
                m_context.session().undo();
            }
        } else if (kind == QStringLiteral("redo")) {
            for (int i = 0; i < steps; ++i) {
                m_context.session().redo();
            }
        }
    });
}

} // namespace cakery
