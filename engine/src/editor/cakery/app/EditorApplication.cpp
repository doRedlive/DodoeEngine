// do@Redlive

#include "EditorApplication.h"
#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/shell/EditorWindow.h"
#include "cakery/app/project_selection/ProjectManagerWindow.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace cakery {

EditorApplication::EditorApplication(int& argc, char** argv, const QString& applicationName,
                                     std::unique_ptr<IEditorBackend> backend)
    : QApplication(argc, argv), m_applicationName(applicationName)
{
    setOrganizationName("Redlive");
    setApplicationName(m_applicationName);

    const QString appEditorDir = QDir(applicationDirPath()).filePath("resources/editor");
    QString builtinEditorDir = appEditorDir;
    if (!QDir(builtinEditorDir).exists()) {
        builtinEditorDir = QDir(applicationDirPath()).absoluteFilePath("../../engine/res/editor");
    }
    m_resources = std::make_unique<EditorResourceLocator>(builtinEditorDir.toStdString());

    const auto themePath = m_resources->resolve("editor://themes/cakery-dark.qss");
    QFile styleFile(QString::fromStdString(themePath.string()));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) setStyleSheet(styleFile.readAll());

    m_session = std::make_unique<EditorSession>(std::move(backend));
    m_workspace = std::make_unique<EditorWorkspaceContext>(*m_session, *m_resources);
}

EditorApplication::~EditorApplication()
{
    if (m_editorWindow) {
        m_editorWindow->close();
        delete m_editorWindow;
        m_editorWindow = nullptr;
    }
    if (m_projectWindow) {
        m_projectWindow->close();
        delete m_projectWindow;
        m_projectWindow = nullptr;
    }
    m_workspace.reset();
    m_session.reset();
    m_resources.reset();
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

    m_editorWindow = new EditorWindow(*m_workspace);
    m_editorWindow->show();
    m_editorWindow->enterWorkspace(projectPath);
}

} // namespace cakery
