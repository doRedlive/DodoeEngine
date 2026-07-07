// do@Redlive

#include "CakeryApplication.h"
#include "MainWindow.h"
#include "services/EngineManager.h"
#include "project/ProjectManagerWindow.h"

#include <QDir>
#include <QFile>

namespace cakery {

static void loadStyleSheet(QApplication& app)
{
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/resources/style.qss";

    for (const auto& path : searchPaths) {
        if (QFile::exists(path)) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString styleSheet = QString::fromUtf8(file.readAll());
                app.setStyleSheet(styleSheet);
                LOG_INFO("[Cakery] Loaded stylesheet from: {}", path.toStdString());
                return;
            }
        }
    }
    LOG_WARN("[Cakery] Could not find style.qss in any search path");
}

CakeryApplication::CakeryApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("Cakery");
    setApplicationVersion("1.0.0");
    setOrganizationName("Redlive");

    loadStyleSheet(*this);

    connect(this, &QApplication::aboutToQuit, [] {
        EngineManager::getInstance().shutdown();
    });
}

CakeryApplication::~CakeryApplication()
{
}

int CakeryApplication::run()
{
    ProjectManagerWindow pmWindow;
    QString selectedProjectPath;
    connect(&pmWindow, &ProjectManagerWindow::onProjectOpened, [&](const QString& path) {
        selectedProjectPath = path;
    });

    if (pmWindow.exec() != QDialog::Accepted || selectedProjectPath.isEmpty()) {
        return 0;
    }

    m_mainWindow = new MainWindow();
    m_mainWindow->show();
    m_mainWindow->enterWorkspace(selectedProjectPath);

    return exec();
}

} // namespace cakery
