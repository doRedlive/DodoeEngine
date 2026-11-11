// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "HistoryPanel.h"

#include "presentation/history/HistoryViewModel.h"

#include "ui/common/EditorIcons.h"
#include "ui/common/EditorWorkspaceContext.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace cakery {

HistoryPanel::~HistoryPanel() = default;

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
    m_undo->setIcon(editorThemedIcon(QStringLiteral("undo.svg")));
    m_undo->setToolTip(tr("Undo"));
    toolsLayout->addWidget(m_undo);

    m_redo = new QToolButton(tools);
    m_redo->setObjectName(QStringLiteral("historyToolButton"));
    m_redo->setIcon(editorThemedIcon(QStringLiteral("redo.svg")));
    m_redo->setToolTip(tr("Redo"));
    toolsLayout->addWidget(m_redo);

    auto* clear = new QToolButton(tools);
    clear->setObjectName(QStringLiteral("historyToolButton"));
    clear->setIcon(editorThemedIcon(QStringLiteral("trash.svg")));
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

    m_viewModel = create_scope<HistoryViewModel>(m_context.session());
    m_viewModelConnection = ScopedConnection(
        m_viewModel->changed,
        m_viewModel->changed.connect([this]() { onViewModelChanged(); }));
    onViewModelChanged();
}

void HistoryPanel::onViewModelChanged()
{
    refresh();
}

void HistoryPanel::refresh()
{
    const HistorySnapshot& snapshot = m_viewModel->snapshot();
    const QString needle = m_search->text().trimmed();
    m_list->clear();

    for (auto it = snapshot.redoLabels.rbegin(); it != snapshot.redoLabels.rend(); ++it) {
        const QString label = ToQString(*it);
        if (!needle.isEmpty() && !label.contains(needle, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(label, m_list);
        item->setForeground(QColor("#6f6f6f"));
    }
    if (!snapshot.redoLabels.empty() && needle.isEmpty()) {
        auto* separator = new QListWidgetItem(QStringLiteral("────"), m_list);
        separator->setFlags(Qt::NoItemFlags);
        separator->setForeground(QColor("#4f4f4f"));
    }
    for (auto it = snapshot.undoLabels.rbegin(); it != snapshot.undoLabels.rend(); ++it) {
        const QString label = ToQString(*it);
        if (!needle.isEmpty() && !label.contains(needle, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(label, m_list);
        item->setForeground(QColor("#d8d8d8"));
    }
    m_undo->setEnabled(snapshot.canUndo);
    m_redo->setEnabled(snapshot.canRedo);
}

void HistoryPanel::undo()
{
    m_viewModel->undo();
}

void HistoryPanel::redo()
{
    m_viewModel->redo();
}

void HistoryPanel::clear()
{
    m_viewModel->clear();
}

} // namespace cakery
