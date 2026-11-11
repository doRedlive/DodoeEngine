// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "EditorApplication.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/ThemeController.h"
#include "ui/shell/EditorWindow.h"
#include "services/ProjectConfig.h"
#include "ProjectSelectionController.h"
#include "core/console/CommandRegistry.h"
#include "core/console/McpAutostart.h"
#include "services/EditorConfig.h"

#include "runtime/core/debug/instrumentor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QTranslator>
#include <QStandardPaths>

namespace cakery {

EditorApplication::EditorApplication(int& argc, char** argv, const QString& applicationName)
    : QApplication(argc, argv), m_applicationName(applicationName)
{
    setupApplication();
    setupTranslations();
    setupResources();
    loadConfiguration();
    setupSession(argc, argv);
    setupWorkspace();
    setupProjectSelection();
}

void EditorApplication::setupApplication()
{
    setOrganizationName("Redlive");
    setApplicationName(m_applicationName);
}

void EditorApplication::setupTranslations()
{
    m_translator = create_scope<QTranslator>();
    QString language = qEnvironmentVariable("DODOE_LANGUAGE");
    if (language.isEmpty()) {
        language = QLocale::system().name();
    }
    const QString catalog = language.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
        ? QStringLiteral("Cakery_zh_CN")
        : QStringLiteral("Cakery_en");
    if (m_translator->load(QStringLiteral(":/i18n/") + catalog + QStringLiteral(".qm"))) {
        installTranslator(m_translator.get());
    }
}

void EditorApplication::setupResources()
{
    const QString appEditorDir = QDir(applicationDirPath()).filePath("resources/editor");
    QString builtinEditorDir = appEditorDir;
    if (!QDir(builtinEditorDir).exists()) {
        builtinEditorDir = QDir(applicationDirPath()).absoluteFilePath("../../engine/res/editor");
    }

    QString applicationIconPath = QDir(applicationDirPath())
        .filePath("resources/pictures/Dodoe-White.jpg");
    if (!QFileInfo::exists(applicationIconPath)) {
        applicationIconPath = QDir(applicationDirPath())
            .absoluteFilePath("../../engine/res/pictures/Dodoe-White.jpg");
    }
    if (QFileInfo::exists(applicationIconPath)) {
        setWindowIcon(QIcon(applicationIconPath));
    }

    m_resources = create_scope<EditorResourceLocator>(ToDodoeString(builtinEditorDir));
    m_theme = create_scope<ThemeController>(*m_resources);
    m_config = create_scope<EditorConfig>();
    m_projectConfig = create_scope<ProjectConfig>();
}

void EditorApplication::loadConfiguration()
{
    const QString userEditorDir = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath("Editor");
    m_config->load(ToDodoeString(m_resources->packagedRoot().string()), {},
                   ToDodoeString(userEditorDir));
    applyTheme(ToQString(m_config->themeName()));
}

void EditorApplication::setupSession(int argc, char** argv)
{
    m_session = create_scope<EditorSession>();
    m_session->attachInspectorConfig(&m_config->inspectorsJson());
    RegisterBuiltinCommands(m_session->commands());
    m_mcpAutostart = create_scope<McpAutostart>(argc, argv);
}

void EditorApplication::setupWorkspace()
{
    m_workspace = create_scope<EditorWorkspaceContext>(*m_session, *m_resources, *m_theme,
                                                       *m_config);
}

void EditorApplication::setupProjectSelection()
{
    m_projectSelection = create_scope<ProjectSelectionController>(*m_projectConfig);
    connect(m_projectSelection.get(), &ProjectSelectionController::projectSelected,
            this, &EditorApplication::onProjectSelected);
}

void EditorApplication::applyTheme(const QString& themeName)
{
    if (m_theme) {
        m_theme->apply(ToDodoeString(themeName));
    }
}

EditorApplication::~EditorApplication()
{
    if (m_editorWindow) {
        m_editorWindow->close();
        delete m_editorWindow;
        m_editorWindow = nullptr;
    }
    m_projectSelection.reset();
    m_workspace.reset();
    m_theme.reset();
    m_session.reset();
    m_resources.reset();
    m_config.reset();
    m_projectConfig.reset();
}

int EditorApplication::run()
{
    m_projectSelection->show();
    return exec();
}

void EditorApplication::onReturnToProjectManager()
{
    if (!m_editorWindow) {
        return;
    }
    const bool quitOnClose = quitOnLastWindowClosed();
    setQuitOnLastWindowClosed(false);
    const bool closed = m_editorWindow->close();
    if (closed) {
        delete m_editorWindow;
        m_editorWindow = nullptr;
        m_projectSelection->show();
    }
    setQuitOnLastWindowClosed(quitOnClose);
}

void EditorApplication::onProjectSelected(const QString& projectPath)
{
    const QFileInfo selectedInfo(projectPath);
    const QString projectRoot = selectedInfo.isDir()
        ? projectPath
        : selectedInfo.absolutePath();
    loadProjectConfiguration(projectRoot);
    applyTheme(ToQString(m_config->themeName()));

    m_editorWindow = new EditorWindow(*m_workspace);
    connect(m_editorWindow, &EditorWindow::returnToProjectManagerRequested,
            this, &EditorApplication::onReturnToProjectManager);
    if (!m_editorWindow->enterWorkspace(projectPath)) {
        delete m_editorWindow;
        m_editorWindow = nullptr;
        return;
    }

    m_projectSelection->dismiss();
    m_editorWindow->show();
    m_mcpAutostart->startIfConfigured(*m_session, *m_config);
}

void EditorApplication::loadProjectConfiguration(const QString& projectRoot)
{
    const QString projectEditorDir = QDir(projectRoot).filePath("ProjectSettings/Editor");
    const QString userEditorDir = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath("Editor");
    m_config->load(ToDodoeString(m_resources->packagedRoot().string()),
                   ToDodoeString(projectEditorDir),
                   ToDodoeString(userEditorDir));
}

} // namespace cakery
