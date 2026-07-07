// do@Redlive

#include "app/CakeryApplication.h"
#include "runtime/function/log/log_system.h"

#include <QtGlobal>
#include <QDateTime>

#ifdef _WIN32
#include <windows.h>
#endif

static void debugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray local = msg.toLocal8Bit();
    QByteArray level;
    dodoe::LogLevel logLevel;
    switch (type) {
    case QtDebugMsg:    level = "DBG"; logLevel = dodoe::LogLevel::Debug; break;
    case QtInfoMsg:     level = "INF"; logLevel = dodoe::LogLevel::Info;  break;
    case QtWarningMsg:  level = "WRN"; logLevel = dodoe::LogLevel::Warn;  break;
    case QtCriticalMsg: level = "CRT"; logLevel = dodoe::LogLevel::Error; break;
    case QtFatalMsg:    level = "FTL"; logLevel = dodoe::LogLevel::Critical; break;
    }

    dodoe::Log::ClientLog(logLevel, msg.toStdString());

    QString formatted = QString("[%1][%2] %3\n")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"))
        .arg(QLatin1String(level))
        .arg(msg);
    QByteArray out = formatted.toLocal8Bit();
#ifdef _WIN32
    OutputDebugStringA(out.constData());
#endif
    fprintf(stderr, "%s", out.constData());
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(debugMessageHandler);

    cakery::CakeryApplication app(argc, argv);
    return app.run();
}
