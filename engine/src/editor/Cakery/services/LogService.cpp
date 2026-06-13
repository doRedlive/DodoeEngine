// do@Redlive

#include "LogService.h"

namespace cakery {

LogService& LogService::getInstance()
{
    static LogService s_instance;
    return s_instance;
}

void LogService::log(LogLevel level, const QString& message)
{
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = QDateTime::currentDateTime();
    entry.sequence = m_sequence++;

    if (!m_entries.isEmpty()) {
        auto& last = m_entries.last();
        if (last.level == level && last.message == message) {
            last.repeatCount++;
            return;
        }
    }

    m_entries.append(entry);
    if (level >= LogLevel::Error) m_errorCount++;
    else if (level == LogLevel::Warn) m_warnCount++;

    while (m_entries.size() > kMaxEntries)
        m_entries.removeFirst();

    emit entryAdded(entry);
}

void LogService::info(const QString& src, const QString& msg)
{
    log(LogLevel::Info, QString("[%1] %2").arg(src, msg));
}

void LogService::warn(const QString& src, const QString& msg)
{
    log(LogLevel::Warn, QString("[%1] %2").arg(src, msg));
}

void LogService::error(const QString& src, const QString& msg)
{
    log(LogLevel::Error, QString("[%1] %2").arg(src, msg));
}

void LogService::clear()
{
    m_entries.clear();
    m_errorCount = 0;
    m_warnCount = 0;
    m_sequence = 0;
    emit cleared();
}

} // namespace cakery
