// do@Redlive

#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class QListWidget;
class QTimer;

namespace ads {
class CDockManager;
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

    void enterWorkspace(const QString& projectPath);

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
    SceneSurface* m_sceneSurface = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ProjectPanel* m_projectPanel = nullptr;
    QListWidget* m_console = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QTimer* m_safePointTimer = nullptr;
    QLabel* m_statusLabel = nullptr;
    bool m_closed = false;
};

} // namespace cakery
