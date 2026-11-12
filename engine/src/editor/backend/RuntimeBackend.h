// do@Redlive

#pragma once

#include "dopch.h"

#include "services/AssetDatabase.h"
#include "services/TilePaintService.h"
#include "core/bridge/EditorTypes.h"
#include "core/bridge/EditorCommands.h"
#include "core/document/EditorDocument.h"
#include "scene/SceneGizmoController.h"
#include "runtime/core/channel/pick_channel.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

#include <cstdint>
#include <future>

namespace dodoe {
    class Application;
    class EditorCameraProvider;
    class IndexedCameraProvider;
    class Entity;
    class RenderViewTarget;
    class Scene;
    class World;
}

namespace cakery {

class EditorCamera;
class EditorSession;
class FieldAttributeRegistry;

class RuntimeBackend final {
public:
    explicit RuntimeBackend(EditorSession& session);
    ~RuntimeBackend();

    RuntimeBackend(const RuntimeBackend&) = delete;
    RuntimeBackend& operator=(const RuntimeBackend&) = delete;

    BackendCapabilities capabilities() const;
    bool openProject(const ProjectDescriptor& project);
    bool openDocument(const String& documentId);
    String startScenePath() const;
    bool execute(const EditorOperation& command);
    bool inspectComponent(const String& typeName,
                          DynamicArray<InspectorFieldMetadata>& fields) const;
    bool listAssets(DynamicArray<AssetBrowserEntry>& entries) const;
    bool getAssetImportSettings(const String& path, AssetImportSettings& settings) const;
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
    void setEventCallback(std::function<void(const BackendEventMessage&)>);
    void setInspectorConfig(const Json* inspectors);

    bool assetRefreshPending() const;
    void assetRefreshProgress(Size_t& done, Size_t& total) const;
    bool queryAssetThumbnail(const String& path, int size, Json& out) const;
    bool queryFrameStats(Json& out) const;
    bool queryFps(int& outFps) const;
    bool queryRenderGraphSnapshot(Json& out) const;
    bool queryRenderSettings(Json& out) const;
    bool findMissingAssetReferences(DynamicArray<UInt64>& out) const;
    bool attachSceneSurface(const SceneSurfaceDescriptor& surface);
    bool bootEngine();
    bool applyRenderSettingsOverlay(const Json& renderSettings);
    bool runBenchmark(int frames, Json& out);
    bool startBenchmark(int frames);
    bool pollBenchmark(Json& out);
    bool queryBenchmarkComparison(Json& out) const;
    void requestSceneSurfaceResize(const ViewportMetrics& metrics);
    bool detachSceneSurface();
    void tickAtSafePoint();
    void requestScriptReload();
    void shutdown();
    BackendStatus status() const;
    String diagnostic() const;

private:
    bool handleCommand(const DocumentChangedCommand& command);
    bool handleCommand(const SceneMouseDownCommand& command);
    bool handleCommand(const SceneMouseMoveCommand& command);
    bool handleCommand(const SceneMouseUpCommand& command);
    bool handleCommand(const SceneMouseWheelCommand& command);
    bool handleCommand(const SceneKeyCommand& command);
    bool handleCommand(const SelectionChangedCommand& command);
    bool handleCommand(const GizmoModeCommand& command);
    bool handleCommand(const GizmoSnapCommand& command);
    bool handleCommand(const GizmoSnapStepCommand& command);
    bool handleCommand(const CameraModeCommand& command);
    bool handleCommand(const SceneImportAssetCommand& command);
    bool handleCommand(const PrefabExportCommand& command);
    bool handleCommand(const PlayActionCommand& command);
    bool handleCommand(const AssetSaveAllCommand& command);
    bool handleCommand(const AssetRefreshCommand& command);
    bool handleCommand(const AssetImportCommand& command);
    bool handleCommand(const AssetReimportCommand& command);
    bool handleCommand(const ScriptToolActionCommand& command);
    bool handleCommand(const AssetUpdateSettingsCommand& command);

    bool bootRuntime();
    void applyPendingMetrics();
    void processScriptReload();
    bool reconcileScene(const EditorDocument& document);
    void rebuildHierarchy(dodoe::Scene& scene, const EditorDocument& document);
    void updateSceneOverlays();
    SceneGizmoContext sceneGizmoContext() const;
    void pickAt(float screenX, float screenY);
    void requestPick(float screenX, float screenY);
    void setSelectedUuid(UInt64 uuid);
    bool importDroppedAsset(const String& assetPath, const Json& position);
    void setPlayAction(PlayAction action);
    dodoe::Entity selectedSceneEntity() const;
    dodoe::World* runtimeWorld() const;
    void reportMissingAssetReferences();

    Scope<dodoe::Application> m_app;
    Scope<EditorCamera> m_camera;
    Scope<dodoe::EditorCameraProvider> m_cameraProvider;
    dodoe::RenderViewTarget* m_sceneTarget = nullptr;
    std::function<void(const BackendEventMessage&)> m_eventCallback;
    ProjectDescriptor m_project;
    SceneSurfaceDescriptor m_surface;
    ViewportMetrics m_pending;
    EditorDocument m_document;
    Scope<AssetDatabase> m_assetDatabase;
    UInt64 m_selectedUuid = 0;
    UInt64 m_pick_sequence = 0;
    bool m_pickToggle = false;
    bool m_pickAdd = false;
    SceneGizmoController m_sceneGizmos;
    bool m_ctrlHeld = false;
    bool m_shiftHeld = false;
    bool m_altHeld = false;
    String m_playState = "edit";
    EditorSession* m_session = nullptr;
    const Json* m_inspectorConfig = nullptr;
    Scope<FieldAttributeRegistry> m_fieldAttributes;
    bool m_tilePaintActive = false;
    bool m_hasDocument = false;
    bool m_hasPendingMetrics = false;
    bool m_booted = false;
    bool m_modulesInitialized = false;
    bool m_scriptReloadPending = false;
    std::future<bool> m_scriptBuild;
    Json m_lastBenchmark;
    Json m_previousBenchmark;
    bool m_benchmarkRunning = false;
    int m_benchmarkFramesRequested = 0;
    UInt64 m_benchmarkStartFrame = 0;
    UInt64 m_benchmarkDeadlineMs = 0;
    BackendState m_state = BackendState::Created;
    String m_diagnostic;
};

} // namespace cakery
