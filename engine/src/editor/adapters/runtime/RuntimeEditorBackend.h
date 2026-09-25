// do@Redlive

#pragma once

#include "adapters/runtime/services/AssetDatabase.h"
#include "adapters/runtime/services/TilePaintService.h"
#include "bridge/EditorBackend.h"
#include "core/document/EditorDocument.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

class RuntimeEditorBackend final : public IEditorBackend {
public:
    RuntimeEditorBackend();
    ~RuntimeEditorBackend() override;

    RuntimeEditorBackend(const RuntimeEditorBackend&) = delete;
    RuntimeEditorBackend& operator=(const RuntimeEditorBackend&) = delete;

    BackendCapabilities capabilities() const override;
    bool openProject(const ProjectDescriptor& project) override;
    bool openDocument(const std::string& documentId) override;
    std::string startScenePath() const override;
    bool execute(const EditorCommandMessage& command) override;
    bool inspectComponent(const std::string& typeName,
                          std::vector<InspectorFieldMetadata>& fields) const override;
    bool listAssets(std::vector<AssetBrowserEntry>& entries) const override;
    bool getAssetImportSettings(const std::string& path, AssetImportSettings& settings) const override;
    bool listLogs(std::vector<BackendLogEntry>& entries) const override;
    bool clearLogs() override;
    bool listToolActions(std::vector<std::string>& actions) const override;
    bool invokeToolAction(const std::string& path) override;
    bool getCustomInspectorUI(const std::string& typeName, nlohmann::json& out) const override;
    bool listEditorWindows(std::vector<std::pair<std::string, std::string>>& out) const override;
    bool getEditorWindowUI(const std::string& id, nlohmann::json& out) const override;
    bool dispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                             const std::string& controlId, const std::string& eventName,
                             const nlohmann::json& value) override;
    void setEventCallback(std::function<void(const BackendEventMessage&)>) override;
    void setEditorSession(EditorSession* session) override;
    bool assetRefreshPending() const override;
    void assetRefreshProgress(std::size_t& done, std::size_t& total) const override;
    bool queryTilemapState(const std::string& tilemapUuid, nlohmann::json& out) const override;
    bool queryAssetThumbnail(const std::string& path, int size, nlohmann::json& out) const override;
    bool findMissingAssetReferences(std::vector<std::uint64_t>& out) const override;
    bool attachSceneSurface(const SceneSurfaceDescriptor& surface) override;
    bool bootEngine() override;
    void requestSceneSurfaceResize(const ViewportMetrics& metrics) override;
    bool detachSceneSurface() override;
    void tickAtSafePoint() override;
    void shutdown() override;
    BackendStatus status() const override;
    std::string diagnostic() const override;

private:
    using CommandHandler = bool (RuntimeEditorBackend::*)(const EditorCommandMessage&);

    void registerCommandHandlers();
    bool handleDocumentChanged(const EditorCommandMessage& command);
    bool handleSceneMouseDown(const EditorCommandMessage& command);
    bool handleSceneMouseMove(const EditorCommandMessage& command);
    bool handleSceneMouseUp(const EditorCommandMessage& command);
    bool handleSceneMouseWheel(const EditorCommandMessage& command);
    bool handleSceneKey(const EditorCommandMessage& command);
    bool handleSelectionChanged(const EditorCommandMessage& command);
    bool handleGizmoMode(const EditorCommandMessage& command);
    bool handleGizmoSnap(const EditorCommandMessage& command);
    bool handleGizmoSnapStep(const EditorCommandMessage& command);
    bool handleCameraMode(const EditorCommandMessage& command);
    bool handleSceneImportAsset(const EditorCommandMessage& command);
    bool handlePrefabExport(const EditorCommandMessage& command);
    bool handlePlayAction(const EditorCommandMessage& command);
    bool handleAssetSaveAll(const EditorCommandMessage& command);
    bool handleAssetRefresh(const EditorCommandMessage& command);
    bool handleAssetImport(const EditorCommandMessage& command);
    bool handleAssetReimport(const EditorCommandMessage& command);
    bool handleScriptToolAction(const EditorCommandMessage& command);
    bool handleAssetUpdateSettings(const EditorCommandMessage& command);

    bool bootRuntime();
    bool executeTilemapCommand(const EditorCommandMessage& command);
    void applyPendingMetrics();
    bool reconcileScene(const EditorDocument& document);
    void rebuildHierarchy(dodoe::Scene& scene, const EditorDocument& document);
    void updateGizmo();
    void updateTileOverlay();
    void pickAt(float screenX, float screenY);
    bool importDroppedAsset(const std::string& assetPath, const nlohmann::json& position);
    void setPlayAction(const std::string& action);
    dodoe::Entity selectedSceneEntity() const;
    dodoe::Entity dragEntityByUuid(std::uint64_t uuid) const;
    int hitTestGizmo(float screenX, float screenY);
    void beginDrag(int axis, float screenX, float screenY);
    void updateDrag(float screenX, float screenY);
    void endDrag();
    struct TransformUpdate {
        std::uint64_t uuid = 0;
        dodoe::Vector3f position;
        dodoe::Vector3f rotation;
        dodoe::Vector3f scale;
    };
    void emitTransformChange(const dodoe::Vector3f& position, const dodoe::Vector3f& rotation,
                             const dodoe::Vector3f& scale);
    void emitTransformChanges(const std::vector<TransformUpdate>& updates);
    void updateTileEditFromSelection();
    void activateTilemapEdit(const dodoe::UUID& tilemapUuid, const dodoe::UUID& layerUuid);
    bool screenToCell(float screenX, float screenY, int& outX, int& outY) const;
    void executeTilemapLayerField(dodoe::UUID layer, const std::string& field,
                                  const nlohmann::json& value);
    dodoe::Entity activeTilemapEntity() const;
    void emitTilemapEditMode(bool active);
    dodoe::World* runtimeWorld() const;
    void reportMissingAssetReferences();

    std::unique_ptr<dodoe::Application> m_app;
    std::unordered_map<std::string, CommandHandler> m_commandHandlers;
    std::unique_ptr<EditorCamera> m_camera;
    std::unique_ptr<dodoe::EditorCameraProvider> m_cameraProvider;
    dodoe::RenderViewTarget* m_sceneTarget = nullptr;
    std::function<void(const BackendEventMessage&)> m_eventCallback;
    ProjectDescriptor m_project;
    SceneSurfaceDescriptor m_surface;
    ViewportMetrics m_pending;
    EditorDocument m_document;
    std::unique_ptr<AssetDatabase> m_assetDatabase;
    std::uint64_t m_selectedUuid = 0;
    std::string m_gizmoMode = "translate";
    bool m_snapEnabled = false;
    float m_translateSnap = 0.25f;
    float m_rotateSnap = 15.0f;
    float m_scaleSnap = 0.1f;
    bool m_ctrlHeld = false;
    bool m_shiftHeld = false;
    bool m_altHeld = false;
    std::string m_playState = "edit";
    EditorSession* m_session = nullptr;
    std::unique_ptr<TilePaintService> m_tilePaint;
    bool m_tilePaintActive = false;
    int m_dragAxis = -1;
    std::string m_dragMode;
    std::vector<TransformUpdate> m_dragEntities;
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
    BackendState m_state = BackendState::Created;
    std::string m_diagnostic;
};

} // namespace cakery
