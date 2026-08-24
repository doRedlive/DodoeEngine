// do@Redlive

#pragma once

#include <QMainWindow>

#include "core/Signal.h"

class QAction;
class QEvent;
class QListWidget;
class QMenuBar;
class QPoint;
class QShowEvent;
class QTimer;
class QToolBar;
class QToolButton;

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace cakery {

class EditorWorkspaceContext;
class HierarchyPanel;
class HistoryPanel;
class InspectorPanel;
class ProjectPanel;
class SceneSurface;

class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit EditorWindow(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~EditorWindow() override;

    bool enterWorkspace(const QString& projectPath);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void createTitleBar();
    void createToolbar();
    void createMenus();
    void createDocks();
    void createPanels();
    void startSafePointTimer();
    void refreshUndoRedoActions();
    void toggleMaximize();
    bool overInteractiveChild(const QPoint& pos) const;
    bool overMenuAction(QMenuBar* menuBar, const QPoint& localPos) const;

    EditorWorkspaceContext& m_context;
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockWidget* m_sceneDock = nullptr;
    SceneSurface* m_sceneSurface = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    HistoryPanel* m_historyPanel = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ProjectPanel* m_projectPanel = nullptr;
    QListWidget* m_console = nullptr;
    QWidget* m_titleBar = nullptr;
    QToolBar* m_editorToolbar = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QToolButton* m_maxButton = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QTimer* m_safePointTimer = nullptr;
    bool m_closed = false;
    ScopedConnection m_historySubscription;
};

} // namespace cakery
