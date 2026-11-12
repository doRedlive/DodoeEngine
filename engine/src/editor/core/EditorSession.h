// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorTypes.h"
#include "core/benchmark/BenchmarkRunner.h"
#include "core/application/DomainEventBus.h"
#include "core/application/EditorCommandBus.h"
#include "core/history/EditHistory.h"
#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/history/EditorHistory.h"
#include "core/EditorSelection.h"
#include "core/PlayState.h"
#include "core/Signal.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

class AICommandBridge;
class CommandRegistry;
class ExecutionRecorder;
class McpServer;
class RuntimeBackend;
class TilemapEditController;
class WorkflowLibrary;

enum class EditorSessionState {
    Created,
    OpeningProject,
    Ready,
    Degraded,
    Failed,
    Closing,
    Closed
};

class EditorSession {
public:
    EditorSession();
    ~EditorSession();

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    bool openProject(ProjectDescriptor project);
    bool openDocument(const String& documentPath);
    bool saveDocument(const String& documentPath);
    bool execute(EditorOperation command);
    bool inspectComponent(const String& typeName,
                          DynamicArray<InspectorFieldMetadata>& fields) const;
    bool listAssets(DynamicArray<AssetBrowserEntry>& entries) const;
    bool getAssetImportSettings(const String& path, AssetImportSettings& settings) const;
    bool attachSceneSurface(SceneSurfaceDescriptor surface);
    bool bootEngine();
    bool applyRenderSettingsOverlay(const Json& renderSettings);
    bool runBenchmark(int frames, Json& out);
    bool startBenchmark(int frames, std::function<void(bool, Json)> onFinished,
                        std::function<void(int, int)> onProgress);
    bool queryBenchmarkComparison(Json& out) const;
    void submitViewportMetrics(ViewportMetrics metrics);
    void attachInspectorConfig(const Json* inspectors);
    void tick();
    void requestScriptReload();
    const String& scriptReloadState() const { return m_scriptReloadState; }
    void shutdown();

    EditorSessionState state() const;
    DomainEventBus& events() { return m_events; }
    const DomainEventBus& events() const { return m_events; }
    EditorCommandBus& commandBus() { return m_commandBus; }
    McpServer& mcpServer() { return *m_mcp; }
    TilemapEditController& tilemapEdit() { return *m_tilemapEdit; }
    const TilemapEditController& tilemapEdit() const { return *m_tilemapEdit; }
    ExecutionRecorder& executionRecorder() { return *m_recorder; }
    AICommandBridge& aiBridge() { return *m_aiBridge; }
    WorkflowLibrary& workflows() { return *m_workflows; }
    CommandRegistry& commands() { return *m_commands; }
    const ProjectDescriptor& project() const;
    FsPath assetRoot() const;
    bool newScene(const FsPath& directory, const String& name);
    BackendCapabilities capabilities() const;
    BackendStatus status() const;
    String diagnostic() const;
    bool listLogs(DynamicArray<BackendLogEntry>& entries) const;
    bool clearLogs();
    bool listToolActions(DynamicArray<String>& actions) const;
    bool invokeToolAction(const String& path);
    bool getCustomInspectorUI(const String& typeName, Json& out) const;
    bool listEditorWindows(DynamicArray<Pair<String, String>>& out) const;
    bool getEditorWindowUI(const String& id, Json& out) const;
    bool dispatchEditorEvent(const String& owner, const String& ownerId,
                             const String& controlId, const String& eventName,
                             const Json& value);

    EditorDocumentModel& documentModel() { return m_documentModel; }
    const EditorDocumentModel& documentModel() const { return m_documentModel; }
    EditorSelection& selection() { return m_selection; }
    const EditorSelection& selection() const { return m_selection; }
    EditorHistory& history() { return m_history; }
    const EditorHistory& history() const { return m_history; }
    EditHistory& editHistory() { return m_editHistory; }
    const EditHistory& editHistory() const { return m_editHistory; }

    UInt64 createEntity(const String& name);
    bool deleteEntity(UInt64 uuid);
    bool deleteEntities(const DynamicArray<UInt64>& uuids);
    bool copyEntities(const DynamicArray<UInt64>& uuids);
    bool cutEntities(const DynamicArray<UInt64>& uuids);
    bool pasteEntities();
    bool duplicateEntities(const DynamicArray<UInt64>& uuids);
    bool hasEntityClipboard() const { return !m_clipboard.empty(); }
    bool renameEntity(UInt64 uuid, const String& name);
    bool reparentEntity(UInt64 uuid, UInt64 newParent);
    bool addComponent(UInt64 uuid, const EditorComponent& component);
    bool removeComponent(UInt64 uuid, Size_t nativeIndex);
    bool moveComponent(UInt64 uuid, Size_t nativeIndex, int delta);
    bool updateComponent(UInt64 uuid, Size_t nativeIndex, const Json& value);
    bool updateComponentOnEntities(const DynamicArray<UInt64>& uuids, const String& typeName,
                                   const Json& value, bool managed);
    bool updateComponentFieldOnEntities(const DynamicArray<UInt64>& uuids,
                                        const String& typeName, const String& fieldPath,
                                        const Json& value, bool managed);
    bool removeManagedComponent(UInt64 uuid, Size_t index);
    bool updateManagedComponent(UInt64 uuid, Size_t index, const Json& value);
    bool undo();
    bool redo();
    void notifyDocumentChanged();

    const String& cameraMode() const { return m_cameraMode; }
    const String& gizmoMode() const { return m_gizmoMode; }
    Signal<String> cameraModeChanged;
    Signal<String> gizmoModeChanged;

    PlayState playState() const { return m_playState; }
    bool playDocumentEdited() const { return m_playDocumentEdited; }
    bool stopPlay(bool keepPlayChanges);
    Signal<PlayState> playStateChanged;
    bool findMissingAssetReferences(DynamicArray<UInt64>& out) const;
    Signal<Size_t> missingAssetReferencesDetected;

    Signal<bool> tileEditModeChanged;
    Signal<> assetDatabaseChanged;
    bool isAssetRefreshPending() const;
    void assetRefreshProgress(Size_t& done, Size_t& total) const;
    bool queryTilemapState(const String& tilemapUuid, Json& out) const;
    bool queryAssetThumbnail(const String& path, int size, Json& out) const;
    bool queryFrameStats(Json& out) const;
    bool queryFps(int& out) const;
    bool queryRenderGraphSnapshot(Json& out) const;
    bool queryRenderSettings(Json& out) const;

private:
    bool canEditDocument() const;
    void handleBackendEvent(const BackendEventMessage& event);
    void onPlayStateChanged(const String& state);
    void applyTransformChange(UInt64 uuid, const Json& value);
    DynamicArray<EditorEntity> snapshotEntitySubtrees(const DynamicArray<UInt64>& roots) const;
    DynamicArray<EditorEntity> remapEntityClones(const DynamicArray<EditorEntity>& snapshot,
                                                bool keepExternalParent) const;

    Scope<CommandRegistry> m_commands;
    Scope<TilemapEditController> m_tilemapEdit;
    Scope<RuntimeBackend> m_backend;
    Scope<McpServer> m_mcp;
    Scope<ExecutionRecorder> m_recorder;
    Scope<AICommandBridge> m_aiBridge;
    Scope<WorkflowLibrary> m_workflows;
    ProjectDescriptor m_project;
    EditorSessionState m_state = EditorSessionState::Created;
    DomainEventBus m_events;
    EditorCommandBus m_commandBus;
    EditorDocumentModel m_documentModel;
    EditorSelection m_selection;
    EditorHistory m_history;
    EditHistory m_editHistory{m_history, m_documentModel};
    ScopedConnection m_selectionSubscription;
    DynamicArray<ScopedConnection> m_eventRepublishConnections;
    UInt64 m_lastViewportSequence = 0;
    bool m_surfaceAttached = false;
    bool m_hasPendingViewportMetrics = false;
    ViewportMetrics m_pendingViewportMetrics;
    String m_cameraMode{"3d"};
    String m_gizmoMode{"translate"};
    bool m_transformDragging = false;
    DynamicArray<EditorEntity> m_clipboard;
    PlayState m_playState = PlayState::Edit;
    EditorDocument m_playSnapshot;
    bool m_hasPlaySnapshot = false;
    bool m_playSnapshotDirty = false;
    bool m_playDocumentEdited = false;
    bool m_pendingStopKeep = false;
    ScopedConnection m_playDocumentSubscription;
    BenchmarkRunner m_benchmark;
    String m_scriptReloadState = "idle";
};

} // namespace cakery
