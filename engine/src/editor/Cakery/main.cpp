// do@Redlive

#include "app/CakeryApplication.h"

#include <QtGlobal>
#include <QDateTime>

#ifdef _WIN32
#include <windows.h>
#endif

static void debugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray local = msg.toLocal8Bit();
    QByteArray level;
    switch (type) {
    case QtDebugMsg:    level = "DBG"; break;
    case QtInfoMsg:     level = "INF"; break;
    case QtWarningMsg:  level = "WRN"; break;
    case QtCriticalMsg: level = "CRT"; break;
    case QtFatalMsg:    level = "FTL"; break;
    }
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
