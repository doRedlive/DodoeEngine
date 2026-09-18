// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"
#include "core/history/EditHistory.h"
#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/history/EditorHistory.h"
#include "core/EditorSelection.h"
#include "core/Signal.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

enum class EditorSessionState {
    Created,
    OpeningProject,
    Ready,
    Degraded,
    Failed,
    Closing,
    Closed
};

enum class PlayState {
    Edit,
    Playing,
    Paused
};

class EditorSession {
public:
    explicit EditorSession(std::unique_ptr<IEditorBackend> backend);
    ~EditorSession();

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    bool openProject(ProjectDescriptor project);
    bool openDocument(const std::string& documentPath);
    bool saveDocument(const std::string& documentPath);
    bool execute(EditorCommandMessage command);
    bool inspectComponent(const std::string& typeName,
                          std::vector<InspectorFieldMetadata>& fields) const;
    bool listAssets(std::vector<AssetBrowserEntry>& entries) const;
    bool getAssetImportSettings(const std::string& path, AssetImportSettings& settings) const;
    bool attachSceneSurface(SceneSurfaceDescriptor surface);
    bool bootEngine();
    void submitViewportMetrics(ViewportMetrics metrics);
    void tick();
    void shutdown();

    EditorSessionState state() const;
    const ProjectDescriptor& project() const;
    std::filesystem::path assetRoot() const;
    bool newScene(const std::filesystem::path& directory, const std::string& name);
    BackendCapabilities capabilities() const;
    BackendStatus status() const;
    std::string diagnostic() const;
    bool listLogs(std::vector<BackendLogEntry>& entries) const;
    bool clearLogs();
    bool listToolActions(std::vector<std::string>& actions) const;
    bool invokeToolAction(const std::string& path);
    bool getCustomInspectorUI(const std::string& typeName, nlohmann::json& out) const;
    bool listEditorWindows(std::vector<std::pair<std::string, std::string>>& out) const;
    bool getEditorWindowUI(const std::string& id, nlohmann::json& out) const;
    bool dispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                             const std::string& controlId, const std::string& eventName,
                             const nlohmann::json& value);

    EditorDocumentModel& documentModel() { return m_documentModel; }
    const EditorDocumentModel& documentModel() const { return m_documentModel; }
    EditorSelection& selection() { return m_selection; }
    const EditorSelection& selection() const { return m_selection; }
    EditorHistory& history() { return m_history; }
    const EditorHistory& history() const { return m_history; }
    EditHistory& editHistory() { return m_editHistory; }
    const EditHistory& editHistory() const { return m_editHistory; }

    std::uint64_t createEntity(const std::string& name);
    bool deleteEntity(std::uint64_t uuid);
    bool deleteEntities(const std::vector<std::uint64_t>& uuids);
    bool copyEntities(const std::vector<std::uint64_t>& uuids);
    bool cutEntities(const std::vector<std::uint64_t>& uuids);
    bool pasteEntities();
    bool duplicateEntities(const std::vector<std::uint64_t>& uuids);
    bool hasEntityClipboard() const { return !m_clipboard.empty(); }
    bool renameEntity(std::uint64_t uuid, const std::string& name);
    bool reparentEntity(std::uint64_t uuid, std::uint64_t newParent);
    bool addComponent(std::uint64_t uuid, const EditorComponent& component);
    bool removeComponent(std::uint64_t uuid, std::size_t nativeIndex);
    bool moveComponent(std::uint64_t uuid, std::size_t nativeIndex, int delta);
    bool updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value);
    bool updateComponentOnEntities(const std::vector<std::uint64_t>& uuids, const std::string& typeName,
                                   const nlohmann::json& value, bool managed);
    bool removeManagedComponent(std::uint64_t uuid, std::size_t index);
    bool updateManagedComponent(std::uint64_t uuid, std::size_t index, const nlohmann::json& value);
    bool undo();
    bool redo();
    void notifyDocumentChanged();

    const std::string& cameraMode() const { return m_cameraMode; }
    Signal<std::string> cameraModeChanged;

    PlayState playState() const { return m_playState; }
    bool playDocumentEdited() const { return m_playDocumentEdited; }
    bool stopPlay(bool keepPlayChanges);
    Signal<PlayState> playStateChanged;
    bool findMissingAssetReferences(std::vector<std::uint64_t>& out) const;
    Signal<std::size_t> missingAssetReferencesDetected;

    Signal<bool> tileEditModeChanged;
    Signal<> assetDatabaseChanged;
    bool isAssetRefreshPending() const;
    void assetRefreshProgress(std::size_t& done, std::size_t& total) const;
    bool queryTilemapState(const std::string& tilemapUuid, nlohmann::json& out) const;
    bool queryAssetThumbnail(const std::string& path, int size, nlohmann::json& out) const;

private:
    bool canEditDocument() const;
    void handleBackendEvent(const BackendEventMessage& event);
    void onPlayStateChanged(const std::string& state);
    void applyTransformChange(std::uint64_t uuid, const nlohmann::json& value);
    std::vector<EditorEntity> snapshotEntitySubtrees(const std::vector<std::uint64_t>& roots) const;
    std::vector<EditorEntity> remapEntityClones(const std::vector<EditorEntity>& snapshot,
                                                bool keepExternalParent) const;

    std::unique_ptr<IEditorBackend> m_backend;
    ProjectDescriptor m_project;
    EditorSessionState m_state = EditorSessionState::Created;
    EditorDocumentModel m_documentModel;
    EditorSelection m_selection;
    ScopedConnection m_selectionSubscription;
    EditorHistory m_history;
    EditHistory m_editHistory{m_history, m_documentModel};
    std::uint64_t m_lastViewportSequence = 0;
    bool m_surfaceAttached = false;
    bool m_hasPendingViewportMetrics = false;
    ViewportMetrics m_pendingViewportMetrics;
    std::string m_cameraMode{"3d"};
    bool m_transformDragging = false;
    std::vector<EditorEntity> m_clipboard;
    PlayState m_playState = PlayState::Edit;
    EditorDocument m_playSnapshot;
    bool m_hasPlaySnapshot = false;
    bool m_playSnapshotDirty = false;
    bool m_playDocumentEdited = false;
    bool m_pendingStopKeep = false;
    ScopedConnection m_playDocumentSubscription;
};

} // namespace cakery
