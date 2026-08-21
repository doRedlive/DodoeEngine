#pragma once

#include <QObject>
#include <QTimer>
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
};

class LogService : public QObject {
    Q_OBJECT
public:
    static LogService& getInstance();

    void trace(const QString& msg);
    void debug(const QString& msg);
    void info(const QString& msg);
    void warn(const QString& msg);
    void error(const QString& msg);
    void critical(const QString& msg);

    void clear();
    const QList<LogEntry>& entries() const { return m_entries; }

    int errorCount() const { return m_errorCount; }
    int warnCount() const { return m_warnCount; }

signals:
    void updated();

private:
    LogService();

    void syncEngineLogs();

    QList<LogEntry> m_entries;
    int m_errorCount = 0;
    int m_warnCount = 0;

    uint64_t m_lastCoreSeq = 0;
    uint64_t m_lastClientSeq = 0;

    static constexpr int kMaxEntries = 1000;
};

} // namespace cakery
