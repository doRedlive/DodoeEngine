#pragma once

#include <QMainWindow>

class QLabel;
class QToolButton;
class QComboBox;
class QLineEdit;
class QAction;

namespace ads {
class CDockManager;
class CDockWidget;
class CDockAreaWidget;
}

namespace cakery {

class SceneWidget;
class HierarchyWidget;
class InspectorWidget;
class ConsoleWidget;
class ProjectBrowserWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void enterWorkspace(const QString& projectPath);

protected:
    void closeEvent(QCloseEvent* event) override;

signals:
    void workspaceEntered(const QString& projectPath);

private:
    void setupMenuBar();
    void setupMainToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void connectSignals();

    void onPlay();
    void onPause();
    void onStop();

    ads::CDockManager* m_dockManager = nullptr;

    ads::CDockWidget* m_viewportDock = nullptr;
    ads::CDockWidget* m_hierarchyDock = nullptr;
    ads::CDockWidget* m_inspectorDock = nullptr;
    ads::CDockWidget* m_projectDock = nullptr;
    ads::CDockWidget* m_consoleDock = nullptr;

    SceneWidget* m_sceneWidget = nullptr;
    HierarchyWidget* m_hierarchyWidget = nullptr;
    InspectorWidget* m_inspectorWidget = nullptr;
    ConsoleWidget* m_consoleWidget = nullptr;
    ProjectBrowserWidget* m_projectWidget = nullptr;

    QToolBar* m_mainToolBar = nullptr;
    QToolButton* m_btnPlay = nullptr;
    QToolButton* m_btnPause = nullptr;
    QToolButton* m_btnStop = nullptr;

    QToolButton* m_toolHand = nullptr;
    QToolButton* m_toolMove = nullptr;
    QToolButton* m_toolRotate = nullptr;
    QToolButton* m_toolScale = nullptr;
    QToolButton* m_toolRect = nullptr;
    QComboBox* m_shadingCombo = nullptr;
    QToolButton* m_btn2D = nullptr;
    QToolButton* m_btnGizmos = nullptr;
    QLineEdit* m_sceneSearch = nullptr;

    QAction* m_actionNewScene = nullptr;
    QAction* m_actionOpenScene = nullptr;
    QAction* m_actionSaveScene = nullptr;
    QAction* m_actionSaveSceneAs = nullptr;
    QAction* m_actionUndo = nullptr;
    QAction* m_actionRedo = nullptr;
    QAction* m_actionProjectSettings = nullptr;

    QLabel* m_fpsLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    bool m_workspaceActive = false;
    bool m_isPlaying = false;
    bool m_isPaused = false;
};

} // namespace cakery
