// do@Redlive

#pragma once

#include <QApplication>
#include "core/EditorSession.h"
#include "services/EditorResourceLocator.h"

#include <memory>

class QTranslator;

namespace cakery {

class EditorWindow;
class ProjectManagerWindow;
class EditorWorkspaceContext;

class EditorApplication : public QApplication {
    Q_OBJECT
public:
    EditorApplication(int& argc, char** argv, const QString& applicationName,
                      std::unique_ptr<IEditorBackend> backend);
    ~EditorApplication() override;

    int run();
    void applyTheme(const QString& themeName);

private:
    void onProjectSelected(const QString& projectPath);

    QString m_applicationName;
    std::unique_ptr<QTranslator> m_translator;
    std::unique_ptr<EditorSession>   m_session;
    std::unique_ptr<EditorResourceLocator> m_resources;
    std::unique_ptr<EditorWorkspaceContext> m_workspace;
    ProjectManagerWindow*             m_projectWindow = nullptr;
    EditorWindow*                     m_editorWindow  = nullptr;
};

} // namespace cakery
