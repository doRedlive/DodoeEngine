// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "WorkflowPanel.h"

#include "ui/widgets/WorkflowGraphView.h"

#include "ui/common/EditorWorkspaceContext.h"
#include "core/console/CommandRegistry.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/WorkflowTemplate.h"

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

namespace {

constexpr Size_t kMaxVisibleRecords = 256;
constexpr Size_t kMaxGraphNodes = 128;

}

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
                Json args;
                args["uuid"] = ToDodoeString(uuid);
                m_context.session().commands().executeStructured(m_context.session(), "entity.select", args);
            });
    connect(m_list, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showContextMenu(pos); });
    auto& recorderChanged = m_context.session().executionRecorder().changed;
    m_recorderSubscription = ScopedConnection(
        recorderChanged, recorderChanged.connect([this]() { refresh(); }));
    refresh();
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

void WorkflowPanel::refresh()
{
    const bool mutatingOnly = m_filter->currentIndex() == 1;
    const String author = authorFilter();

    const QScrollBar* scrollbar = m_list->verticalScrollBar();
    const bool atBottom = !scrollbar || scrollbar->value() >= scrollbar->maximum() - 2;
    m_list->clear();

    const Json report = m_context.session().executionRecorder().toJSON(kMaxVisibleRecords, mutatingOnly, author);
    if (!report.contains("records") || !report["records"].is_array()) {
        return;
    }
    for (const auto& record : report["records"]) {
        const String command = record.value("command", String());
        const bool marker = command == "session.begin" || command == "session.end";
        const bool ok = record.value("ok", false);
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(0, QString::number(record.value("sequence", UInt64(0))));
        item->setText(1, ToQString(command));
        item->setText(2, ToQString(record.value("author", String())));
        item->setText(3, ok ? QStringLiteral("OK") : QStringLiteral("FAIL"));
        item->setText(4, QString::number(record.value("duration_ms", 0.0), 'f', 1));
        item->setText(5, ToQString(record.value("session", String())));

        QColor statusColor = ok ? QColor("#7fbf7f") : QColor("#e06c6c");
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
        item->setData(0, Qt::UserRole, ToQString(record.dump()));
    }

    if (atBottom) {
        m_list->scrollToBottom();
    }

    const Json graph = m_context.session().executionRecorder().toGraphJSON(kMaxGraphNodes, mutatingOnly, author);
    m_graph->setGraph(graph);
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
    m_context.session().executionRecorder().clear();
}

void WorkflowPanel::refreshTemplates()
{
    m_context.session().workflows().setStoragePath(WorkflowStoragePath(m_context.session()));
    m_template->blockSignals(true);
    m_template->clear();
    for (const String& name : m_context.session().workflows().names()) {
        m_template->addItem(ToQString(name));
    }
    m_template->blockSignals(false);
    m_run->setEnabled(m_template->count() > 0);
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
    const Json report =
        m_context.session().executionRecorder().toJSON(kMaxVisibleRecords, true, authorFilter());
    DynamicArray<WorkflowStep> steps = CaptureWorkflowSteps(
        report.contains("records") ? report["records"] : Json());
    if (steps.empty()) {
        m_details->setPlainText(tr("No replayable commands in the current filter window."));
        return;
    }
    m_context.session().workflows().setStoragePath(WorkflowStoragePath(m_context.session()));
    const Size_t count = steps.size();
    m_context.session().workflows().store(ToDodoeString(name), std::move(steps));
    refreshTemplates();
    const auto* stored = m_context.session().workflows().find(ToDodoeString(name));
    m_details->setPlainText(QStringLiteral("Captured %1 steps into '%2'.")
                                .arg(count)
                                .arg(name) +
                            (stored ? QStringLiteral("\nRun it any time from the template list.")
                                    : QString()));
}

void WorkflowPanel::runTemplate()
{
    const QString name = m_template->currentText();
    if (name.isEmpty()) {
        return;
    }
    m_context.session().workflows().setStoragePath(WorkflowStoragePath(m_context.session()));
    const auto* steps = m_context.session().workflows().find(ToDodoeString(name));
    if (!steps || steps->empty()) {
        return;
    }
    const Json result = RunWorkflow(m_context.session(), ToDodoeString(name), *steps);
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
    const String command = record.value("command", String());
    if (command.empty()) {
        return;
    }
    if (argsOverride) {
        const CommandResult result =
            m_context.session().commands().executeStructured(m_context.session(), command, *argsOverride);
        m_details->setPlainText(ToQString(result.ok
            ? result.message + "\n" + ToString(result.data, 2)
            : result.message));
        return;
    }
    const Json args = record.value("args", Json());
    if (args.is_object()) {
        const CommandResult result =
            m_context.session().commands().executeStructured(m_context.session(), command, args);
        m_details->setPlainText(ToQString(result.ok
            ? result.message + "\n" + ToString(result.data, 2)
            : result.message));
        return;
    }
    if (args.is_string()) {
        const CommandResult result =
            m_context.session().commands().execute(m_context.session(), args.get<String>());
        m_details->setPlainText(ToQString(result.ok
            ? result.message + "\n" + ToString(result.data, 2)
            : result.message));
    }
}

} // namespace cakery
