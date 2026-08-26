// do@Redlive

#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QString>

#include "core/Signal.h"

class QAction;
class QEvent;
class QMenu;
class QMenuBar;
class QPoint;
class QShowEvent;
class QTimer;
class QToolBar;
class QToolButton;

namespace ads {
class CDockManager;
class CDockWidget;
class CFloatingDockContainer;
}

namespace cakery {

class EditorWorkspaceContext;
class ConsolePanel;
class HierarchyPanel;
class HistoryPanel;
class InspectorPanel;
class ProjectPanel;
class SceneSurface;
class SettingsPanel;
class TileLayersPanel;
class TilePalettePanel;

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
    void createToolsMenu();
    void refreshToolsMenu();
    void createDocks();
    void createPanels();
    void createWindowMenu();
    void populatePanelMenus();
    void setupPanelToggle(ads::CDockWidget* dock);
    void setupFloatingDockWindow(ads::CFloatingDockContainer* floating);
    void startSafePointTimer();
    void refreshUndoRedoActions();
    void resetLayout();
    void restoreLayoutState();
    void saveLayoutState() const;
    void toggleMaximize();
    void updateMaximizeButton();
    bool overInteractiveChild(const QPoint& pos) const;
    bool overMenuAction(QMenuBar* menuBar, const QPoint& localPos) const;

    EditorWorkspaceContext& m_context;
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockWidget* m_sceneDock = nullptr;
    ads::CDockWidget* m_hierarchyDock = nullptr;
    ads::CDockWidget* m_inspectorDock = nullptr;
    ads::CDockWidget* m_projectDock = nullptr;
    ads::CDockWidget* m_consoleDock = nullptr;
    ads::CDockWidget* m_terminalDock = nullptr;
    ads::CDockWidget* m_historyDock = nullptr;
    ads::CDockWidget* m_gameSettingsDock = nullptr;
    ads::CDockWidget* m_engineSettingsDock = nullptr;
    ads::CDockWidget* m_tilePaletteDock = nullptr;
    ads::CDockWidget* m_tileLayersDock = nullptr;
    SceneSurface* m_sceneSurface = nullptr;
    HierarchyPanel* m_hierarchy = nullptr;
    HistoryPanel* m_historyPanel = nullptr;
    InspectorPanel* m_inspector = nullptr;
    ProjectPanel* m_projectPanel = nullptr;
    ConsolePanel* m_console = nullptr;
    SettingsPanel* m_gameSettingsPanel = nullptr;
    SettingsPanel* m_engineSettingsPanel = nullptr;
    TilePalettePanel* m_tilePalette = nullptr;
    TileLayersPanel* m_tileLayers = nullptr;
    QWidget* m_titleBar = nullptr;
    QToolBar* m_editorToolbar = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QMenu* m_settingsMenu = nullptr;
    QMenu* m_windowMenu = nullptr;
    QMenu* m_toolsMenu = nullptr;
    QAction* m_resetLayoutAction = nullptr;
    QToolButton* m_maxButton = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_camera2DAction = nullptr;
    QTimer* m_safePointTimer = nullptr;
    bool m_closed = false;
    QByteArray* m_defaultLayoutState = nullptr;
    QString m_layoutStatePath;
    ScopedConnection m_historySubscription;
    ScopedConnection m_cameraModeSubscription;
};

} // namespace cakery
