// do@Redlive

#pragma once

#include "dopch.h"

#include <QApplication>

#include "core/EditorSession.h"
#include "services/EditorResourceLocator.h"

class QTranslator;

namespace cakery {

class EditorWindow;
class EditorWorkspaceContext;
class McpAutostart;
class ProjectSelectionController;
class ThemeController;

class EditorApplication : public QApplication {
    Q_OBJECT
public:
    EditorApplication(int& argc, char** argv, const QString& applicationName);
    ~EditorApplication() override;

    int run();
    void applyTheme(const QString& themeName);

private:
    void setupApplication();
    void setupTranslations();
    void setupResources();
    void loadConfiguration();
    void setupSession(int argc, char** argv);
    void setupWorkspace();
    void setupProjectSelection();

    void onProjectSelected(const QString& projectPath);
    void onReturnToProjectManager();
    void loadProjectConfiguration(const QString& projectRoot);

    QString m_applicationName;
    Scope<QTranslator> m_translator;
    Scope<EditorSession> m_session;
    Scope<EditorResourceLocator> m_resources;
    Scope<ThemeController> m_theme;
    Scope<EditorWorkspaceContext> m_workspace;
    Scope<ProjectSelectionController> m_projectSelection;
    Scope<McpAutostart> m_mcpAutostart;
    EditorWindow* m_editorWindow = nullptr;
};

} // namespace cakery
