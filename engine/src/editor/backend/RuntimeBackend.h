// do@Redlive

#pragma once

#include "dopch.h"

#include "services/AssetDatabase.h"
#include "services/TilePaintService.h"
#include "core/bridge/EditorTypes.h"
#include "core/document/EditorDocument.h"
#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/channel/pick_channel.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

#include <cstdint>

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
    bool queryTilemapState(const String& tilemapUuid, Json& out) const;
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
    bool handleCommand(const TilemapCreateCommand& command);
    bool handleCommand(const TilemapActivateCommand& command);
    bool handleCommand(const TilemapToolCommand& command);
    bool handleCommand(const TilemapBrushFlipCommand& command);
    bool handleCommand(const TilemapBrushRotateCommand& command);
    bool handleCommand(const TilemapRandomBrushCommand& command);
    bool handleCommand(const TilemapSelectionDeleteCommand& command);
    bool handleCommand(const TilemapSelectionCopyCommand& command);
    bool handleCommand(const TilemapSelectionPasteCommand& command);
    bool handleCommand(const TilemapBrushCommand& command);
    bool handleCommand(const TilemapEditTilesetCommand& command);
    bool handleCommand(const TilemapAddTilesetCommand& command);
    bool handleCommand(const TilemapRemoveTilesetCommand& command);
    bool handleCommand(const TilemapResizeCommand& command);
    bool handleCommand(const TilemapLayerAddCommand& command);
    bool handleCommand(const TilemapLayerRemoveCommand& command);
    bool handleCommand(const TilemapLayerMoveCommand& command);
    bool handleCommand(const TilemapLayerActiveCommand& command);
    bool handleCommand(const TilemapLayerVisibleCommand& command);
    bool handleCommand(const TilemapLayerOpacityCommand& command);
    bool handleCommand(const TilemapLayerRenameCommand& command);

    bool bootRuntime();
    void applyPendingMetrics();
    bool reconcileScene(const EditorDocument& document);
    void rebuildHierarchy(dodoe::Scene& scene, const EditorDocument& document);
    void updateGizmo();
    void updateTileOverlay();
    float computeGizmoScale(const dodoe::Vector3f& position) const;
    void drawSelectionHighlight(dodoe::GizmoChannelData& data);
    void pickAt(float screenX, float screenY);
    void requestPick(float screenX, float screenY);
    void setSelectedUuid(UInt64 uuid);
    bool importDroppedAsset(const String& assetPath, const Json& position);
    void setPlayAction(const String& action);
    dodoe::Entity selectedSceneEntity() const;
    dodoe::Entity dragEntityByUuid(UInt64 uuid) const;
    int hitTestGizmo(float screenX, float screenY);
    void beginDrag(int axis, float screenX, float screenY);
    void updateDrag(float screenX, float screenY);
    void endDrag();
    struct TransformUpdate {
        UInt64 uuid = 0;
        dodoe::Vector3f position;
        dodoe::Vector3f rotation;
        dodoe::Vector3f scale;
    };
    void emitTransformChange(const dodoe::Vector3f& position, const dodoe::Vector3f& rotation,
                             const dodoe::Vector3f& scale);
    void emitTransformChanges(const DynamicArray<TransformUpdate>& updates);
    void updateTileEditFromSelection();
    void activateTilemapEdit(const dodoe::UUID& tilemapUuid, const dodoe::UUID& layerUuid);
    bool screenToCell(float screenX, float screenY, int& outX, int& outY) const;
    void executeTilemapLayerField(dodoe::UUID layer, const String& field,
                                  const Json& value);
    dodoe::Entity activeTilemapEntity() const;
    void emitTilemapEditMode(bool active);
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
    String m_gizmoMode = "translate";
    float m_gizmoScale = 1.0f;
    int m_hoverAxis = -1;
    bool m_snapEnabled = false;
    float m_translateSnap = 0.25f;
    float m_rotateSnap = 15.0f;
    float m_scaleSnap = 0.1f;
    bool m_ctrlHeld = false;
    bool m_shiftHeld = false;
    bool m_altHeld = false;
    String m_playState = "edit";
    EditorSession* m_session = nullptr;
    const Json* m_inspectorConfig = nullptr;
    Scope<TilePaintService> m_tilePaint;
    Scope<FieldAttributeRegistry> m_fieldAttributes;
    bool m_tilePaintActive = false;
    int m_dragAxis = -1;
    String m_dragMode;
    DynamicArray<TransformUpdate> m_dragEntities;
    dodoe::Vector3f m_dragStartPosition{0.0f, 0.0f, 0.0f};
    dodoe::Vector3f m_dragStartRotation{0.0f, 0.0f, 0.0f};
    dodoe::Vector3f m_dragStartScale{1.0f, 1.0f, 1.0f};
    dodoe::Vector3f m_dragPlanePoint{0.0f, 0.0f, 0.0f};
    float m_dragStartAngle = 0.0f;
    float m_dragStartAxisParam = 0.0f;
    bool m_hasDocument = false;
    bool m_hasPendingMetrics = false;
    bool m_booted = false;
    bool m_modulesInitialized = false;
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
