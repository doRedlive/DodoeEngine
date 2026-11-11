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
    copy->setIcon(editorIcon(QStringLiteral("copy.svg")));
    copy->setToolTip(tr("Copy selected logs"));
    toolsLayout->addWidget(copy);

    auto* clear = new QToolButton(tools);
    clear->setObjectName(QStringLiteral("consoleToolButton"));
    clear->setIcon(editorIcon(QStringLiteral("trash.svg")));
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
    connect(copyShortcut, &QShortcut::activated, this, &ConsolePanel::copySelection);

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
    m_list->clear();
    for (const ConsoleEntry& entry : m_viewModel->entries()) {
        const ConsoleLogLevel level = ToConsoleLogLevel(entry.level);
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
        item->setForeground(levelColor(level));
        item->setToolTip(fullMessage);
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
    case ConsoleLogLevel::Trace: return QColor(QStringLiteral("#7f8c8d"));
    case ConsoleLogLevel::Debug: return QColor(QStringLiteral("#75a7d9"));
    case ConsoleLogLevel::Info: return QColor(QStringLiteral("#d8d8d8"));
    case ConsoleLogLevel::Warning: return QColor(QStringLiteral("#e4b44c"));
    case ConsoleLogLevel::Error: return QColor(QStringLiteral("#e05b5b"));
    case ConsoleLogLevel::Critical: return QColor(QStringLiteral("#ff6680"));
    }
    return QColor(QStringLiteral("#d8d8d8"));
}

} // namespace cakery
