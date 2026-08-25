// do@Redlive

#pragma once

#include "adapters/runtime/services/AssetDatabase.h"
#include "adapters/runtime/services/TilePaintService.h"
#include "bridge/EditorBackend.h"
#include "core/document/EditorDocument.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/uuid.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace dodoe {
    class Application;
    class EditorCameraProvider;
    class IndexedCameraProvider;
    class Entity;
    class RenderViewTarget;
    class Scene;
    class SceneRes;
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
    void setEventCallback(std::function<void(const BackendEventMessage&)>) override;
    void setEditorSession(EditorSession* session) override;
    bool queryTilemapState(const std::string& tilemapUuid, nlohmann::json& out) const override;
    bool attachSceneSurface(const SceneSurfaceDescriptor& surface) override;
    void requestSceneSurfaceResize(const ViewportMetrics& metrics) override;
    bool detachSceneSurface() override;
    void tickAtSafePoint() override;
    void shutdown() override;
    BackendStatus status() const override;
    std::string diagnostic() const override;

private:
    bool bootRuntime();
    bool executeTilemapCommand(const EditorCommandMessage& command);
    void applyPendingMetrics();
    bool reconcileScene(const EditorDocument& document);
    void rebuildHierarchy(dodoe::Scene& scene, const EditorDocument& document);
    void updateGizmo();
    void updateTileOverlay();
    void pickAt(float screenX, float screenY);
    void setPlayAction(const std::string& action);
    dodoe::Entity selectedSceneEntity() const;
    int hitTestGizmo(float screenX, float screenY);
    void beginDrag(int axis, float screenX, float screenY);
    void updateDrag(float screenX, float screenY);
    void endDrag();
    void emitTransformChange(const dodoe::Vector3f& position, const dodoe::Vector3f& rotation,
                             const dodoe::Vector3f& scale);
    void updateTileEditFromSelection();
    void activateTilemapEdit(const dodoe::UUID& tilemapUuid, const dodoe::UUID& layerUuid);
    bool screenToCell(float screenX, float screenY, int& outX, int& outY) const;
    void executeTilemapLayerField(dodoe::UUID layer, const std::string& field,
                                  const nlohmann::json& value);
    dodoe::Entity activeTilemapEntity() const;
    void emitTilemapEditMode(bool active);
    dodoe::World* runtimeWorld() const;

    std::unique_ptr<dodoe::Application> m_app;
    std::unique_ptr<EditorCamera> m_camera;
    std::unique_ptr<dodoe::EditorCameraProvider> m_cameraProvider;
    dodoe::RenderViewTarget* m_sceneTarget = nullptr;
    std::function<void(const BackendEventMessage&)> m_eventCallback;
    std::chrono::steady_clock::time_point m_lastTick;
    ProjectDescriptor m_project;
    SceneSurfaceDescriptor m_surface;
    ViewportMetrics m_pending;
    EditorDocument m_document;
    std::unique_ptr<AssetDatabase> m_assetDatabase;
    std::unique_ptr<dodoe::SceneRes> m_playSnapshot;
    std::uint64_t m_selectedUuid = 0;
    std::string m_gizmoMode = "translate";
    std::string m_playState = "edit";
    EditorSession* m_session = nullptr;
    std::unique_ptr<TilePaintService> m_tilePaint;
    bool m_tilePaintActive = false;
    int m_dragAxis = -1;
    std::string m_dragMode;
    dodoe::Vector3f m_dragStartPosition{0.0f, 0.0f, 0.0f};
    dodoe::Vector3f m_dragStartRotation{0.0f, 0.0f, 0.0f};
    dodoe::Vector3f m_dragStartScale{1.0f, 1.0f, 1.0f};
    dodoe::Vector3f m_dragPlanePoint{0.0f, 0.0f, 0.0f};
    float m_dragStartAngle = 0.0f;
    float m_dragStartAxisParam = 0.0f;
    bool m_hasDocument = false;
    bool m_hasPendingMetrics = false;
    bool m_booted = false;
    BackendState m_state = BackendState::Created;
    std::string m_diagnostic;
};

} // namespace cakery
