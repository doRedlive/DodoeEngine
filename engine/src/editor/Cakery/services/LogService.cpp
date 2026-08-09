#include "LogService.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

LogService& LogService::getInstance()
{
    static LogService s_instance;
    return s_instance;
}

LogService::LogService()
{
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &LogService::syncEngineLogs);
    timer->start(16);
}

void LogService::trace(const QString& msg)
{
    LOG_TRACE("{}", msg.toStdString());
}

void LogService::debug(const QString& msg)
{
    LOG_DEBUG("{}", msg.toStdString());
}

void LogService::info(const QString& msg)
{
    LOG_INFO("{}", msg.toStdString());
}

void LogService::warn(const QString& msg)
{
    LOG_WARN("{}", msg.toStdString());
}

void LogService::error(const QString& msg)
{
    LOG_ERROR("{}", msg.toStdString());
}

void LogService::critical(const QString& msg)
{
    LOG_CRITICAL("{}", msg.toStdString());
}

void LogService::clear()
{
    m_entries.clear();
    m_errorCount = 0;
    m_warnCount = 0;
    m_lastCoreSeq = 0;
    m_lastClientSeq = 0;
}

void LogService::syncEngineLogs()
{
    auto mapLevel = [](dodoe::LogLevel lvl) -> LogLevel {
        switch (lvl) {
        case dodoe::LogLevel::Trace:    return LogLevel::Trace;
        case dodoe::LogLevel::Debug:    return LogLevel::Debug;
        case dodoe::LogLevel::Info:     return LogLevel::Info;
        case dodoe::LogLevel::Warn:     return LogLevel::Warn;
        case dodoe::LogLevel::Error:    return LogLevel::Error;
        case dodoe::LogLevel::Critical: return LogLevel::Critical;
        default: return LogLevel::Trace;
        }
    };

    auto forward = [this, &mapLevel](const dodoe::LogMessage& msg) {
        if (!m_entries.isEmpty()) {
            auto& last = m_entries.last();
            if (last.level == mapLevel(msg.level) && last.message == QString::fromStdString(msg.payload.c_str())) {
                last.repeatCount++;
                return;
            }
        }

        LogEntry entry;
        entry.timestamp = QDateTime::currentDateTime();
        entry.level = mapLevel(msg.level);
        entry.message = QString::fromStdString(msg.payload.c_str());
        entry.repeatCount = 1;
        m_entries.append(entry);

        if (entry.level >= LogLevel::Error) m_errorCount++;
        else if (entry.level == LogLevel::Warn) m_warnCount++;

        while (m_entries.size() > kMaxEntries)
            m_entries.removeFirst();
    };

    bool hasNew = false;

    auto coreLogs = dodoe::Log::GetCoreLogs();
    for (const auto& msg : coreLogs) {
        if (msg.sequence > m_lastCoreSeq) {
            m_lastCoreSeq = msg.sequence;
            forward(msg);
            hasNew = true;
        }
    }

    auto clientLogs = dodoe::Log::GetClientLogs();
    for (const auto& msg : clientLogs) {
        if (msg.sequence > m_lastClientSeq) {
            m_lastClientSeq = msg.sequence;
            forward(msg);
            hasNew = true;
        }
    }

    if (hasNew) {
        emit updated();
    }
}

} // namespace cakery
