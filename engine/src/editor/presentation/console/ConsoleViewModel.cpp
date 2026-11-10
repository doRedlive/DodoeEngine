// do@Redlive

#include "dopch.h"

#include "ConsoleViewModel.h"

#include "core/bridge/EditorTypes.h"

#include <algorithm>

namespace cakery {

ConsoleViewModel::ConsoleViewModel(EditorSession& session)
    : m_session(session)
{
    m_notificationSubscription = m_session.events().subscribe<NotificationEvent>(
        [this](const NotificationEvent& event) { onNotification(event); });
}

void ConsoleViewModel::onNotification(const NotificationEvent& event)
{
    ConsoleEntryLevel level = ConsoleEntryLevel::Info;
    if (event.level == "error") {
        level = ConsoleEntryLevel::Error;
    } else if (event.level == "warning") {
        level = ConsoleEntryLevel::Warning;
    }
    String message = event.message;
    if (!event.source.empty()) {
        message = message + " (" + event.source + ")";
    }
    append(level, message, "Bus");
}

void ConsoleViewModel::append(ConsoleEntryLevel level, const String& message, const String& source)
{
    if (message.empty()) {
        return;
    }
    m_local.push_back({level, message, source});
    rebuildMerged();
    notifyChanged();
}

void ConsoleViewModel::pollBackend()
{
    DynamicArray<BackendLogEntry> logs;
    if (!m_session.listLogs(logs)) {
        return;
    }
    DynamicArray<ConsoleEntry> next;
    next.reserve(logs.size());
    for (const BackendLogEntry& log : logs) {
        ConsoleEntryLevel level = ConsoleEntryLevel::Info;
        switch (log.level) {
        case BackendLogLevel::Trace: level = ConsoleEntryLevel::Trace; break;
        case BackendLogLevel::Debug: level = ConsoleEntryLevel::Debug; break;
        case BackendLogLevel::Info: level = ConsoleEntryLevel::Info; break;
        case BackendLogLevel::Warning: level = ConsoleEntryLevel::Warning; break;
        case BackendLogLevel::Error: level = ConsoleEntryLevel::Error; break;
        case BackendLogLevel::Critical: level = ConsoleEntryLevel::Critical; break;
        }
        String message = log.message;
        if (log.repeatCount > 1) {
            message = message + " (x" + ToString(log.repeatCount) + ")";
        }
        next.push_back({level, message, log.source});
    }

    const bool changed = next.size() != m_backend.size()
        || !std::equal(next.begin(), next.end(), m_backend.begin(),
            [](const ConsoleEntry& lhs, const ConsoleEntry& rhs) {
                return lhs.level == rhs.level && lhs.message == rhs.message &&
                       lhs.source == rhs.source;
            });
    if (!changed) {
        return;
    }
    m_backend = std::move(next);
    rebuildMerged();
    notifyChanged();
}

void ConsoleViewModel::clear()
{
    m_local.clear();
    m_backend.clear();
    m_session.clearLogs();
    rebuildMerged();
    notifyChanged();
}

void ConsoleViewModel::rebuildMerged()
{
    m_entries.clear();
    m_entries.reserve(m_local.size() + m_backend.size());
    for (const ConsoleEntry& entry : m_local) {
        m_entries.push_back(entry);
    }
    for (const ConsoleEntry& entry : m_backend) {
        m_entries.push_back(entry);
    }
}

} // namespace cakery
