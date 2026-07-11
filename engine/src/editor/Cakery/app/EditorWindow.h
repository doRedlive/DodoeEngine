// do@Redlive

#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>
#include <QToolButton>

namespace ads {
    class CDockManager;
    class CDockWidget;
}

namespace cakery {

class EditorContext;
class ScenePanel;
class GamePanel;
class HierarchyPanel;
class InspectorPanel;
class ConsolePanel;
class ProjectPanel;
class TerminalPanel;
class LayoutManager;

class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit EditorWindow(EditorContext& ctx, QWidget* parent = nullptr);
    ~EditorWindow() override;

    void enterWorkspace(const QString& projectPath);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupFrameTimer();
    void connectActions();

    void onPlay();
    void onPause();
    void onStop();
    void onUndo();
    void onRedo();
    void onNewScene();
    void onOpenScene();
    void onSaveScene();

    EditorContext& m_ctx;

    ads::CDockManager* m_dockManager = nullptr;

    ScenePanel*     m_scenePanel     = nullptr;
    GamePanel*      m_gamePanel      = nullptr;
    HierarchyPanel* m_hierarchyPanel = nullptr;
    InspectorPanel* m_inspectorPanel = nullptr;
    ConsolePanel*   m_consolePanel   = nullptr;
    ProjectPanel*   m_projectPanel   = nullptr;
    TerminalPanel*  m_terminalPanel  = nullptr;

    std::unique_ptr<LayoutManager> m_layoutManager;

    QToolButton* m_btnPlay   = nullptr;
    QToolButton* m_btnPause  = nullptr;
    QToolButton* m_btnStop   = nullptr;

    QToolButton* m_toolHand  = nullptr;
    QToolButton* m_toolMove  = nullptr;
    QToolButton* m_toolRotate = nullptr;
    QToolButton* m_toolScale = nullptr;

    QLabel* m_fpsLabel   = nullptr;
    QLabel* m_titleLabel  = nullptr;

    QTimer m_frameTimer;
    QElapsedTimer m_frameClock;

    QAction* m_actionUndo = nullptr;
    QAction* m_actionRedo = nullptr;

    bool m_isPlaying = false;
    bool m_isPaused  = false;
};

} // namespace cakery
