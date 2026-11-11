// do@Redlive

#include "dopch.h"

#include "ConsolePanel.h"

#include "presentation/console/ConsoleViewModel.h"

#include "ui/common/EditorIcons.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/QtStringInterop.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QLabel>
#include <QSignalBlocker>
#include <QItemSelectionModel>
#include <QMap>
#include <QListWidget>
#include <QShortcut>
#include <QScrollBar>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace cakery {

namespace {

ConsoleLogLevel ToConsoleLogLevel(ConsoleEntryLevel level)
{
    switch (level) {
    case ConsoleEntryLevel::Trace: return ConsoleLogLevel::Trace;
    case ConsoleEntryLevel::Debug: return ConsoleLogLevel::Debug;
    case ConsoleEntryLevel::Info: return ConsoleLogLevel::Info;
    case ConsoleEntryLevel::Warning: return ConsoleLogLevel::Warning;
    case ConsoleEntryLevel::Error: return ConsoleLogLevel::Error;
    case ConsoleEntryLevel::Critical: return ConsoleLogLevel::Critical;
    }
    return ConsoleLogLevel::Info;
}

} // namespace

ConsolePanel::~ConsolePanel() = default;

ConsolePanel::ConsolePanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* tools = new QWidget(this);
    tools->setObjectName(QStringLiteral("consoleToolbar"));
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(6);

    m_search = new QLineEdit(tools);
    m_search->setObjectName(QStringLiteral("consoleSearch"));
    m_search->setPlaceholderText(tr("Search logs..."));
    m_search->setClearButtonEnabled(true);
    toolsLayout->addWidget(m_search, 1);

    m_levelFilter = new QComboBox(tools);
    m_levelFilter->setObjectName(QStringLiteral("consoleLevelFilter"));
    m_levelFilter->addItem(tr("All levels"), -1);
    for (int level = static_cast<int>(ConsoleLogLevel::Trace);
         level <= static_cast<int>(ConsoleLogLevel::Critical); ++level) {
        m_levelFilter->addItem(levelName(static_cast<ConsoleLogLevel>(level)), level);
    }
    toolsLayout->addWidget(m_levelFilter);

    auto* copy = new QToolButton(tools);
    copy->setObjectName(QStringLiteral("consoleToolButton"));
    copy->setIcon(editorThemedIcon(QStringLiteral("copy.svg")));
    copy->setToolTip(tr("Copy selected logs"));
    toolsLayout->addWidget(copy);

    auto* clear = new QToolButton(tools);
    clear->setObjectName(QStringLiteral("consoleToolButton"));
    clear->setIcon(editorThemedIcon(QStringLiteral("trash.svg")));
    clear->setToolTip(tr("Clear logs"));
    toolsLayout->addWidget(clear);
    layout->addWidget(tools);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("consoleList"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setTextElideMode(Qt::ElideNone);
    layout->addWidget(m_list, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { refresh(); });
    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { refresh(); });
    connect(copy, &QToolButton::clicked, this, &ConsolePanel::copySelection);
    connect(clear, &QToolButton::clicked, this, &ConsolePanel::clearEntries);
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, m_list);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &ConsolePanel::copySelection);

    m_detail = new QLabel(tr("Select a message to view details."), this);
    m_detail->setObjectName(QStringLiteral("consoleDetail"));
    m_detail->setWordWrap(true);
    m_detail->setMaximumHeight(72);
    m_detail->setTextFormat(Qt::PlainText);
    m_detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_detail);
    connect(m_list, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem* item, QListWidgetItem*) {
            m_detail->setText(item ? item->data(Qt::UserRole).toString()
                : tr("Select a message to view details."));
        });

    m_viewModel = create_scope<ConsoleViewModel>(context.session());
    m_viewModelConnection = ScopedConnection(
        m_viewModel->changed,
        m_viewModel->changed.connect([this]() { onViewModelChanged(); }));
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(250);
    connect(m_refreshTimer, &QTimer::timeout, this, &ConsolePanel::syncBackendLogs);
    m_refreshTimer->start();
    onViewModelChanged();
}

void ConsolePanel::onViewModelChanged()
{
    refresh();
}

void ConsolePanel::append(ConsoleLogLevel level, const QString& message, const QString& source)
{
    if (message.trimmed().isEmpty()) {
        return;
    }
    m_viewModel->append(static_cast<ConsoleEntryLevel>(level), ToDodoeString(message),
                        ToDodoeString(source));
}

void ConsolePanel::refresh()
{
    QScrollBar* scrollbar = m_list->verticalScrollBar();
    const bool atBottom = scrollbar->value() >= scrollbar->maximum() - 4;
    const int scrollPosition = scrollbar->value();

    const QString needle = m_search->text().trimmed();
    const int selectedLevel = m_levelFilter->currentData().toInt();
    // An index alone is not stable when local messages are prepended to backend logs.
    const auto selectedItems = m_list->selectedItems();
    QMap<qulonglong, QString> selectedIndices;
    for (const auto* item : selectedItems) selectedIndices.insert(item->data(Qt::UserRole + 1).toULongLong(), item->text());
    const QString currentText = m_list->currentItem() ? m_list->currentItem()->text() : QString();
    const qulonglong currentIndex = m_list->currentItem()
        ? m_list->currentItem()->data(Qt::UserRole + 1).toULongLong()
        : static_cast<qulonglong>(-1);
    QSignalBlocker blocker(m_list);
    m_list->clear();
    int warnings = 0;
    int errors = 0;
    qulonglong entryIndex = 0;
    for (const ConsoleEntry& entry : m_viewModel->entries()) {
        const qulonglong sourceIndex = entryIndex++;
        const ConsoleLogLevel level = ToConsoleLogLevel(entry.level);
        if (level == ConsoleLogLevel::Warning) ++warnings;
        if (level == ConsoleLogLevel::Error || level == ConsoleLogLevel::Critical) ++errors;
        if (selectedLevel >= 0 && selectedLevel != static_cast<int>(level)) {
            continue;
        }
        const QString message = ToQString(entry.message);
        const QString source = ToQString(entry.source);
        const QString fullMessage = source.isEmpty()
            ? message
            : QStringLiteral("[%1] %2").arg(source, message);
        if (!needle.isEmpty() && !fullMessage.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(levelName(level), fullMessage), m_list);
        item->setData(Qt::UserRole, fullMessage);
        item->setData(Qt::UserRole + 1, sourceIndex);
        if (sourceIndex == currentIndex && item->text() == currentText) m_list->setCurrentItem(item, QItemSelectionModel::NoUpdate);
        item->setSelected(selectedIndices.value(sourceIndex) == item->text());
        item->setForeground(levelColor(level));
        item->setToolTip(fullMessage);
    }
    blocker.unblock();
    m_detail->setText(m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString()
        : tr("Select a message to view details."));
    if (warnings != m_warningCount || errors != m_errorCount) {
        m_warningCount = warnings;
        m_errorCount = errors;
        emit messageCountsChanged(warnings, errors);
    }

    if (atBottom) {
        QTimer::singleShot(0, this, [this]() {
            QScrollBar* bar = m_list->verticalScrollBar();
            bar->setValue(bar->maximum());
        });
    } else {
        scrollbar->setValue(scrollPosition);
    }
}

void ConsolePanel::syncBackendLogs()
{
    m_viewModel->pollBackend();
}

void ConsolePanel::copySelection()
{
    QStringList lines;
    for (QListWidgetItem* item : m_list->selectedItems()) {
        lines.push_back(item->text());
    }
    if (!lines.isEmpty()) {
        QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    }
}

void ConsolePanel::clearEntries()
{
    m_viewModel->clear();
}

QString ConsolePanel::levelName(ConsoleLogLevel level)
{
    switch (level) {
    case ConsoleLogLevel::Trace: return QStringLiteral("TRACE");
    case ConsoleLogLevel::Debug: return QStringLiteral("DEBUG");
    case ConsoleLogLevel::Info: return QStringLiteral("INFO");
    case ConsoleLogLevel::Warning: return QStringLiteral("WARN");
    case ConsoleLogLevel::Error: return QStringLiteral("ERROR");
    case ConsoleLogLevel::Critical: return QStringLiteral("CRITICAL");
    }
    return QStringLiteral("LOG");
}

QColor ConsolePanel::levelColor(ConsoleLogLevel level)
{
    switch (level) {
    case ConsoleLogLevel::Trace: return QColor(QStringLiteral("#6272a4"));
    case ConsoleLogLevel::Debug: return QColor(QStringLiteral("#8be9fd"));
    case ConsoleLogLevel::Info: return QColor(QStringLiteral("#f8f8f2"));
    case ConsoleLogLevel::Warning: return QColor(QStringLiteral("#f1fa8c"));
    case ConsoleLogLevel::Error: return QColor(QStringLiteral("#ff5555"));
    case ConsoleLogLevel::Critical: return QColor(QStringLiteral("#ff79c6"));
    }
    return QColor(QStringLiteral("#f8f8f2"));
}

} // namespace cakery
