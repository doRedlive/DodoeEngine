// do@Redlive

#include "EditorApplication.h"
#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/shell/EditorWindow.h"
#include "cakery/app/project_selection/ProjectManagerWindow.h"
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

EditorApplication::EditorApplication(int& argc, char** argv, const QString& applicationName,
                                     std::unique_ptr<IEditorBackend> backend)
    : QApplication(argc, argv), m_applicationName(applicationName)
{
    DO_PROFILE_BEGIN_SESSION("Cakery", "CakeryProfile.json");
    setOrganizationName("Redlive");
    setApplicationName(m_applicationName);

    m_translator = std::make_unique<QTranslator>();
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
    m_resources = std::make_unique<EditorResourceLocator>(builtinEditorDir.toStdString());
    const QString userEditorDir = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath("Editor");
    EditorConfig::self().load(builtinEditorDir.toStdString(), {}, userEditorDir.toStdString());

    applyTheme(QString::fromStdString(EditorConfig::self().themeName()));

    m_session = std::make_unique<EditorSession>(std::move(backend));
    m_workspace = std::make_unique<EditorWorkspaceContext>(*m_session, *m_resources);
}

void EditorApplication::applyTheme(const QString& themeName)
{
    const QString normalized = themeName.trimmed().isEmpty()
        ? QStringLiteral("cakery-dark")
        : themeName.trimmed();
    const auto basePath = m_resources->resolve("editor://themes/cakery-dark.qss");
    QFile baseFile(QString::fromStdString(basePath.string()));
    if (!baseFile.open(QFile::ReadOnly | QFile::Text)) {
        return;
    }

    QString styleSheet = QString::fromUtf8(baseFile.readAll());
    if (normalized != QStringLiteral("cakery-dark")) {
        const auto overridePath = m_resources->resolve(
            (std::string("editor://themes/") + normalized.toStdString() + ".qss").c_str());
        QFile overrideFile(QString::fromStdString(overridePath.string()));
        if (overrideFile.open(QFile::ReadOnly | QFile::Text)) {
            styleSheet += QStringLiteral("\n") + QString::fromUtf8(overrideFile.readAll());
        }
    }
    setStyleSheet(styleSheet);
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
    DO_PROFILE_END_SESSION();
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
    const QFileInfo selectedInfo(projectPath);
    const QString projectRoot = selectedInfo.isDir()
        ? projectPath
        : selectedInfo.absolutePath();
    const QString projectEditorDir = QDir(projectRoot).filePath("ProjectSettings/Editor");
    const QString userEditorDir = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath("Editor");
    EditorConfig::self().load(m_resources->packagedRoot().string(),
                              projectEditorDir.toStdString(),
                              userEditorDir.toStdString());
    applyTheme(QString::fromStdString(EditorConfig::self().themeName()));
    m_editorWindow = new EditorWindow(*m_workspace);
    if (!m_editorWindow->enterWorkspace(projectPath)) {
        delete m_editorWindow;
        m_editorWindow = nullptr;
        return;
    }

    if (m_projectWindow) {
        m_projectWindow->close();
        m_projectWindow->deleteLater();
        m_projectWindow = nullptr;
    }
    m_editorWindow->show();
}

} // namespace cakery
