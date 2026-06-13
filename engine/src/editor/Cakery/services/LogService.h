// do@Redlive

#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>

namespace cakery {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString message;
    int repeatCount = 1;
    int sequence = 0;
};

class LogService : public QObject {
    Q_OBJECT
public:
    static LogService& getInstance();

    void log(LogLevel level, const QString& message);
    void info(const QString& src, const QString& msg);
    void warn(const QString& src, const QString& msg);
    void error(const QString& src, const QString& msg);

    void clear();
    const QList<LogEntry>& entries() const { return m_entries; }

    int errorCount() const { return m_errorCount; }
    int warnCount() const { return m_warnCount; }

signals:
    void entryAdded(const LogEntry& entry);
    void cleared();

private:
    LogService() = default;

    QList<LogEntry> m_entries;
    int m_errorCount = 0;
    int m_warnCount = 0;
    int m_sequence = 0;

    static constexpr int kMaxEntries = 1000;
};

} // namespace cakery
