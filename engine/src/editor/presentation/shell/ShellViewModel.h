// do@Redlive
#pragma once
#include "dopch.h"

#include "core/PlayState.h"
#include "core/Signal.h"
#include "core/bridge/EditorCommands.h"
#include "core/bridge/EditorTypes.h"
#include "presentation/ViewModel.h"

namespace cakery {

class AssetController;
class DocumentController;
class EditorViewController;
class PlayModeController;
class RuntimeController;
class ScriptWindowController;
class ToolActionController;

class ShellViewModel : public ViewModel {
public:
    ShellViewModel(DocumentController& documents, PlayModeController& play,
                   EditorViewController& view, ToolActionController& tools,
                   ScriptWindowController& scriptWindows, AssetController& assets,
                   RuntimeController& runtime, const String& applicationName);
    ~ShellViewModel() override;

    PlayState playState() const { return m_playState; }
    bool canUndo() const { return m_canUndo; }
    bool canRedo() const { return m_canRedo; }
    bool is2D() const { return m_is2D; }
    GizmoMode gizmoMode() const { return m_gizmoMode; }
    bool hasDocument() const { return m_hasDocument; }
    bool isDirty() const { return m_dirty; }
    bool playDocumentEdited() const;
    String windowTitle() const;
    String documentPath() const;
    String diagnostic() const;

    bool assetRefreshPending() const;
    void assetRefreshProgress(Size_t& done, Size_t& total) const;

    void tick();
    void shutdown();
    void undo();
    void redo();
    void play();
    void pauseOrResume();
    void stopPlay(bool keepChanges);
    void setCamera2D(bool ortho);
    void setGizmoMode(GizmoMode mode);
    void setGizmoSnap(bool enabled);
    void setGizmoSnapStep(float translate, float rotate, float scale);

    bool saveDocument(const String& path);
    bool openProject(ProjectDescriptor project);
    bool bootEngine();
    bool applyRenderSettingsOverlay(const Json& renderSettings);
    bool listToolActions(DynamicArray<String>& out) const;
    bool invokeToolAction(const String& path);
    bool listEditorWindows(DynamicArray<Pair<String, String>>& out) const;
    bool getEditorWindowUI(const String& id, Json& out) const;
    bool dispatchEditorEvent(const String& owner, const String& ownerId,
                             const String& controlId, const String& eventName,
                             const Json& value);

    Signal<> historyChanged;
    Signal<> runtimeChanged;
    Signal<> cameraChanged;
    Signal<GizmoMode> gizmoChanged;
    Signal<> documentChanged;
    Signal<Size_t> missingAssetReferences;

private:
    void refreshHistory();
    void refreshRuntime();
    void refreshCamera();
    void refreshGizmo();
    void refreshDocument();

    DocumentController& m_documents;
    PlayModeController& m_play;
    EditorViewController& m_view;
    ToolActionController& m_tools;
    ScriptWindowController& m_scriptWindows;
    AssetController& m_assets;
    RuntimeController& m_runtime;
    String m_applicationName;
    PlayState m_playState = PlayState::Edit;
    bool m_canUndo = false;
    bool m_canRedo = false;
    bool m_is2D = false;
    GizmoMode m_gizmoMode = GizmoMode::Translate;
    bool m_hasDocument = false;
    bool m_dirty = false;
    String m_sceneName;
    ScopedConnection m_historySubscription;
    ScopedConnection m_playSubscription;
    ScopedConnection m_cameraSubscription;
    ScopedConnection m_gizmoSubscription;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_missingSubscription;
};

} // namespace cakery
