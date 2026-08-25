// do@Redlive

#include "ConsolePanel.h"

#include "cakery/ui/EditorIcons.h"
#include "cakery/ui/EditorWorkspaceContext.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QShortcut>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace cakery {

ConsolePanel::ConsolePanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
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

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(250);
    connect(m_refreshTimer, &QTimer::timeout, this, &ConsolePanel::syncBackendLogs);
    m_refreshTimer->start();
    syncBackendLogs();
}

void ConsolePanel::append(ConsoleLogLevel level, const QString& message, const QString& source)
{
    if (message.trimmed().isEmpty()) {
        return;
    }
    m_entries.push_back({level, message, source});
    refresh();
}

void ConsolePanel::refresh()
{
    const QString needle = m_search->text().trimmed();
    const int selectedLevel = m_levelFilter->currentData().toInt();
    m_list->clear();
    const auto addEntry = [this, &needle, selectedLevel](const Entry& entry) {
        if (selectedLevel >= 0 && selectedLevel != static_cast<int>(entry.level)) {
            return;
        }
        const QString fullMessage = entry.source.isEmpty()
            ? entry.message
            : QStringLiteral("[%1] %2").arg(entry.source, entry.message);
        if (!needle.isEmpty() && !fullMessage.contains(needle, Qt::CaseInsensitive)) {
            return;
        }
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(levelName(entry.level), fullMessage), m_list);
        item->setData(Qt::UserRole, fullMessage);
        item->setForeground(levelColor(entry.level));
        item->setToolTip(fullMessage);
    };
    for (const Entry& entry : m_entries) {
        addEntry(entry);
    }
    for (const Entry& entry : m_backendEntries) {
        addEntry(entry);
    }
}

void ConsolePanel::syncBackendLogs()
{
    std::vector<BackendLogEntry> logs;
    if (!m_context.session().listLogs(logs)) {
        return;
    }
    std::vector<Entry> nextEntries;
    nextEntries.reserve(logs.size());
    for (const BackendLogEntry& log : logs) {
        ConsoleLogLevel level = ConsoleLogLevel::Info;
        switch (log.level) {
        case BackendLogLevel::Trace: level = ConsoleLogLevel::Trace; break;
        case BackendLogLevel::Debug: level = ConsoleLogLevel::Debug; break;
        case BackendLogLevel::Info: level = ConsoleLogLevel::Info; break;
        case BackendLogLevel::Warning: level = ConsoleLogLevel::Warning; break;
        case BackendLogLevel::Error: level = ConsoleLogLevel::Error; break;
        case BackendLogLevel::Critical: level = ConsoleLogLevel::Critical; break;
        }
        const QString message = log.repeatCount > 1
            ? QStringLiteral("%1 (x%2)").arg(QString::fromStdString(log.message)).arg(log.repeatCount)
            : QString::fromStdString(log.message);
        nextEntries.push_back({level, message, QString::fromStdString(log.source)});
    }
    const bool changed = nextEntries.size() != m_backendEntries.size()
        || !std::equal(nextEntries.begin(), nextEntries.end(), m_backendEntries.begin(),
            [](const Entry& lhs, const Entry& rhs) {
                return lhs.level == rhs.level && lhs.message == rhs.message && lhs.source == rhs.source;
            });
    if (changed) {
        m_backendEntries = std::move(nextEntries);
        refresh();
    }
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
    m_entries.clear();
    m_backendEntries.clear();
    m_context.session().clearLogs();
    refresh();
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
