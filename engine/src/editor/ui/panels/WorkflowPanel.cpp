// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "WorkflowPanel.h"

#include "presentation/workflow/WorkflowViewModel.h"

#include "ui/widgets/WorkflowGraphView.h"

#include "ui/common/EditorWorkspaceContext.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace cakery {

WorkflowPanel::WorkflowPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* tools = new QWidget(this);
    tools->setObjectName(QStringLiteral("workflowToolbar"));
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(6);

    m_filter = new QComboBox(tools);
    m_filter->setObjectName(QStringLiteral("workflowFilter"));
    m_filter->addItem(tr("All commands"));
    m_filter->addItem(tr("Mutating only"));
    m_filter->setCurrentIndex(1);
    toolsLayout->addWidget(m_filter);

    m_author = new QComboBox(tools);
    m_author->setObjectName(QStringLiteral("workflowAuthor"));
    m_author->addItem(tr("Anyone"));
    m_author->addItem(tr("User"));
    m_author->addItem(tr("AI"));
    m_author->addItem(tr("Workflow"));
    toolsLayout->addWidget(m_author);

    auto* capture = new QToolButton(tools);
    capture->setObjectName(QStringLiteral("workflowToolButton"));
    capture->setText(tr("Capture"));
    capture->setToolTip(tr("Capture filtered history as a reusable workflow template"));
    toolsLayout->addWidget(capture);

    m_template = new QComboBox(tools);
    m_template->setObjectName(QStringLiteral("workflowTemplate"));
    m_template->setToolTip(tr("Captured workflow templates"));
    toolsLayout->addWidget(m_template, 1);

    m_run = new QToolButton(tools);
    m_run->setObjectName(QStringLiteral("workflowToolButton"));
    m_run->setText(tr("Run"));
    m_run->setToolTip(tr("Replay the selected workflow template"));
    toolsLayout->addWidget(m_run);
    m_run->setEnabled(false);

    toolsLayout->addStretch(1);

    auto* clear = new QToolButton(tools);
    clear->setObjectName(QStringLiteral("workflowToolButton"));
    clear->setText(tr("Clear"));
    clear->setToolTip(tr("Clear execution history"));
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName(QStringLiteral("workflowSplitter"));

    m_graph = new WorkflowGraphView(splitter);
    m_graph->setObjectName(QStringLiteral("workflowGraph"));
    splitter->addWidget(m_graph);

    m_list = new QTreeWidget(splitter);
    m_list->setObjectName(QStringLiteral("workflowList"));
    m_list->setHeaderLabels({tr("#"), tr("Command"), tr("Author"), tr("Status"), tr("ms"), tr("Session")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int column = 2; column < 6; ++column) {
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    splitter->addWidget(m_list);

    m_details = new QPlainTextEdit(splitter);
    m_details->setObjectName(QStringLiteral("workflowDetails"));
    m_details->setReadOnly(true);
    m_details->setMaximumHeight(160);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    layout->addWidget(splitter, 1);

    connect(m_filter, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(m_author, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(clear, &QToolButton::clicked, this, &WorkflowPanel::clearHistory);
    connect(capture, &QToolButton::clicked, this, &WorkflowPanel::captureTemplate);
    connect(m_run, &QToolButton::clicked, this, &WorkflowPanel::runTemplate);
    connect(m_template, &QComboBox::currentIndexChanged, this, [this]() {
        m_run->setEnabled(m_template->currentIndex() >= 0);
    });
    connect(m_list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
                showDetails(current);
                if (current) {
                    bool ok = false;
                    const UInt64 seq = current->text(0).toULongLong(&ok);
                    if (ok) {
                        m_graph->highlightNode(seq);
                    }
                }
            });
    connect(m_graph, &WorkflowGraphView::nodeSelected, this,
            [this](UInt64 sequence) { selectRecord(sequence); });
    connect(m_graph, &WorkflowGraphView::nodeActivated, this,
            [this](UInt64, const QString& uuid) {
                m_viewModel->selectEntity(ToDodoeString(uuid));
            });
    connect(m_list, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showContextMenu(pos); });

    m_viewModel = create_scope<WorkflowViewModel>(m_context.session());
    m_viewModelConnection = ScopedConnection(
        m_viewModel->changed,
        m_viewModel->changed.connect([this]() { onViewModelChanged(); }));
    refresh();
    const DynamicArray<String>& templates = m_viewModel->templates();
    for (const String& name : templates) {
        m_template->addItem(ToQString(name));
    }
    m_run->setEnabled(m_template->count() > 0);
}

String WorkflowPanel::authorFilter() const
{
    if (m_author->currentIndex() == 1) {
        return "user";
    }
    if (m_author->currentIndex() == 2) {
        return "ai";
    }
    if (m_author->currentIndex() == 3) {
        return "workflow";
    }
    return "all";
}

void WorkflowPanel::onViewModelChanged()
{
    refresh();
}

void WorkflowPanel::refresh()
{
    m_viewModel->setQuery(m_filter->currentIndex() == 1, authorFilter());

    const QScrollBar* scrollbar = m_list->verticalScrollBar();
    const bool atBottom = !scrollbar || scrollbar->value() >= scrollbar->maximum() - 2;
    m_list->clear();

    for (const WorkflowRecordRow& row : m_viewModel->records()) {
        const bool marker = row.command == "session.begin" || row.command == "session.end";
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, QString::number(row.sequence));
        item->setText(1, ToQString(row.command));
        item->setText(2, ToQString(row.author));
        item->setText(3, row.ok ? QStringLiteral("OK") : QStringLiteral("FAIL"));
        item->setText(4, QString::number(row.durationMs, 'f', 1));
        item->setText(5, ToQString(row.session));

        QColor statusColor = row.ok ? QColor("#7fbf7f") : QColor("#e06c6c");
        QColor baseColor("#d8d8d8");
        if (marker) {
            statusColor = QColor("#9a9a9a");
            baseColor = QColor("#9a9a9a");
            QFont markerFont = item->font(1);
            markerFont.setItalic(true);
            item->setFont(1, markerFont);
        }
        item->setForeground(3, statusColor);
        for (int column = 0; column < 6; ++column) {
            if (column != 3) {
                item->setForeground(column, baseColor);
            }
        }
        item->setData(0, Qt::UserRole, ToQString(row.raw));
    }

    if (atBottom) {
        m_list->scrollToBottom();
    }

    m_graph->setGraph(m_viewModel->graph());
}

void WorkflowPanel::selectRecord(const UInt64 sequence)
{
    QTreeWidgetItemIterator it(m_list);
    while (*it) {
        bool ok = false;
        const UInt64 seq = (*it)->text(0).toULongLong(&ok);
        if (ok && seq == sequence) {
            m_list->setCurrentItem(*it);
            m_list->scrollToItem(*it);
            return;
        }
        ++it;
    }
}

void WorkflowPanel::showDetails(QTreeWidgetItem* current)
{
    if (!current) {
        m_details->clear();
        return;
    }
    const QString raw = current->data(0, Qt::UserRole).toString();
    const Json parsed = Json::parse(ToDodoeString(raw), nullptr, false);
    if (parsed.is_discarded()) {
        m_details->setPlainText(raw);
        return;
    }
    m_details->setPlainText(ToQString(parsed.dump(2)));
}

void WorkflowPanel::clearHistory()
{
    m_viewModel->clearHistory();
}

void WorkflowPanel::captureTemplate()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("Capture workflow"),
                                               tr("Template name:"), QLineEdit::Normal,
                                               QStringLiteral(), &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    String message;
    m_viewModel->captureTemplate(ToDodoeString(name), message);
    m_details->setPlainText(ToQString(message));
    m_template->clear();
    for (const String& templateName : m_viewModel->templates()) {
        m_template->addItem(ToQString(templateName));
    }
    m_run->setEnabled(m_template->count() > 0);
}

void WorkflowPanel::runTemplate()
{
    const QString name = m_template->currentText();
    if (name.isEmpty()) {
        return;
    }
    const Json result = m_viewModel->runTemplate(ToDodoeString(name));
    if (result.is_null()) {
        return;
    }
    m_details->setPlainText(ToQString(result.dump(2)));
}

void WorkflowPanel::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_list->itemAt(pos);
    if (!item) {
        return;
    }
    const Json record =
        Json::parse(ToDodoeString(item->data(0, Qt::UserRole).toString()), nullptr, false);
    if (record.is_discarded() || !record.is_object() ||
        !record.contains("command") || !record["command"].is_string()) {
        return;
    }
    QMenu menu(m_list);
    QAction* rerunAction = menu.addAction(tr("Re-run command"));
    QAction* editAction = menu.addAction(tr("Edit arguments and re-run..."));
    QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == rerunAction) {
        rerunRecord(record, nullptr);
    } else if (chosen == editAction) {
        const Json args = record.value("args", Json::object());
        bool accepted = false;
        const QString text = QInputDialog::getMultiLineText(
            this, tr("Edit arguments"), tr("Arguments JSON:"),
            ToQString(args.is_object() ? args.dump(2) : "{}"), &accepted);
        if (!accepted) {
            return;
        }
        const Json parsed =
            Json::parse(ToDodoeString(text), nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            m_details->setPlainText(tr("Invalid arguments JSON."));
            return;
        }
        rerunRecord(record, &parsed);
    }
}

void WorkflowPanel::rerunRecord(const Json& record, const Json* argsOverride)
{
    const String output = m_viewModel->rerun(record, argsOverride);
    if (!output.empty()) {
        m_details->setPlainText(ToQString(output));
    }
}

} // namespace cakery
