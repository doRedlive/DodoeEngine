// do@Redlive

#include "HistoryPanel.h"

#include "cakery/ui/EditorIcons.h"
#include "cakery/ui/EditorWorkspaceContext.h"
#include "core/history/EditorHistory.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace cakery {

HistoryPanel::HistoryPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* tools = new QWidget(this);
    tools->setObjectName(QStringLiteral("historyToolbar"));
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(6);

    m_search = new QLineEdit(tools);
    m_search->setObjectName(QStringLiteral("historySearch"));
    m_search->setPlaceholderText(tr("Search history..."));
    m_search->setClearButtonEnabled(true);
    toolsLayout->addWidget(m_search, 1);

    m_undo = new QToolButton(tools);
    m_undo->setObjectName(QStringLiteral("historyToolButton"));
    m_undo->setIcon(editorIcon(QStringLiteral("undo.svg")));
    m_undo->setToolTip(tr("Undo"));
    toolsLayout->addWidget(m_undo);

    m_redo = new QToolButton(tools);
    m_redo->setObjectName(QStringLiteral("historyToolButton"));
    m_redo->setIcon(editorIcon(QStringLiteral("redo.svg")));
    m_redo->setToolTip(tr("Redo"));
    toolsLayout->addWidget(m_redo);

    auto* clear = new QToolButton(tools);
    clear->setObjectName(QStringLiteral("historyToolButton"));
    clear->setIcon(editorIcon(QStringLiteral("trash.svg")));
    clear->setToolTip(tr("Clear history"));
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("historyList"));
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_list, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { refresh(); });
    connect(m_undo, &QToolButton::clicked, this, &HistoryPanel::undo);
    connect(m_redo, &QToolButton::clicked, this, &HistoryPanel::redo);
    connect(clear, &QToolButton::clicked, this, &HistoryPanel::clear);
    m_historySubscription = m_context.session().history().subscribe([this]() { refresh(); });
    refresh();
}

void HistoryPanel::refresh()
{
    const EditorHistory& history = m_context.session().history();
    const auto& undoStack = history.undoStack();
    const auto& redoStack = history.redoStack();
    const QString needle = m_search->text().trimmed();
    m_list->clear();

    for (auto it = redoStack.rbegin(); it != redoStack.rend(); ++it) {
        const QString label = QString::fromStdString((*it)->label());
        if (!needle.isEmpty() && !label.contains(needle, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(label, m_list);
        item->setForeground(QColor("#6f6f6f"));
    }
    if (!redoStack.empty() && needle.isEmpty()) {
        auto* separator = new QListWidgetItem(QStringLiteral("────"), m_list);
        separator->setFlags(Qt::NoItemFlags);
        separator->setForeground(QColor("#4f4f4f"));
    }
    for (auto it = undoStack.rbegin(); it != undoStack.rend(); ++it) {
        const QString label = QString::fromStdString((*it)->label());
        if (!needle.isEmpty() && !label.contains(needle, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(label, m_list);
        item->setForeground(QColor("#d8d8d8"));
    }
    m_undo->setEnabled(history.canUndo());
    m_redo->setEnabled(history.canRedo());
}

void HistoryPanel::undo()
{
    m_context.session().undo();
}

void HistoryPanel::redo()
{
    m_context.session().redo();
}

void HistoryPanel::clear()
{
    m_context.session().history().clear();
}

} // namespace cakery
