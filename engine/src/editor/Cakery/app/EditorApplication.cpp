// do@Redlive

#include "EditorApplication.h"
#include "EditorWindow.h"
#include "framework/EditorContext.h"
#include "Cakery/project/ProjectManagerWindow.h"

#include "runtime/function/log/log_system.h"

#include <QDir>
#include <QFileInfo>

namespace cakery {

EditorApplication::EditorApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setOrganizationName("Redlive");
    setApplicationName("Cakery");

    auto appDir = QDir(applicationDirPath());
    QString stylePath = appDir.filePath("resources/style.qss");
    if (QFileInfo::exists(stylePath)) {
        QFile styleFile(stylePath);
        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            setStyleSheet(styleFile.readAll());
        }
    }

    m_ctx = std::make_unique<EditorContext>();
}

EditorApplication::~EditorApplication()
{
    if (m_editorWindow) {
        m_editorWindow->close();
        m_editorWindow->deleteLater();
    }
    if (m_projectWindow) {
        m_projectWindow->close();
        m_projectWindow->deleteLater();
    }
    if (m_ctx->isBooted()) {
        m_ctx->shutdown();
    }
}

int EditorApplication::run()
{
    m_projectWindow = new ProjectManagerWindow();
    connect(m_projectWindow, &ProjectManagerWindow::onProjectOpened,
            this, &EditorApplication::onProjectSelected);
    m_projectWindow->show();

    return exec();
}

void EditorApplication::onProjectSelected(const QString& projectPath)
{
    if (m_projectWindow) {
        m_projectWindow->close();
        m_projectWindow->deleteLater();
        m_projectWindow = nullptr;
    }

    LOG_INFO("[EditorApplication] Opening project: {}", projectPath.toStdString());

    m_editorWindow = new EditorWindow(*m_ctx);
    m_editorWindow->enterWorkspace(projectPath);
    m_editorWindow->show();
}

} // namespace cakery
