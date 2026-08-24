// do@Redlive

#pragma once

#include <QMainWindow>

#include "core/Signal.h"

class QAction;
class QListWidget;
class QTimer;

namespace ads {
class CDockManager;
class CDockWidget;
}

namespace cakery {

class EditorWorkspaceContext;
class HierarchyPanel;
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
    void closeEvent(QCloseEvent* event) override;

private:
    void createMenus();
    void createToolbar();
    void createDocks();
    void createPanels();
    void startSafePointTimer();
    void refreshUndoRedoActions();

    EditorWorkspaceContext& m_context;
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockWidget* m_sceneDock = nullptr;
    SceneSurface* m_sceneSurface = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ProjectPanel* m_projectPanel = nullptr;
    QListWidget* m_console = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QTimer* m_safePointTimer = nullptr;
    bool m_closed = false;
    ScopedConnection m_historySubscription;
};

} // namespace cakery
