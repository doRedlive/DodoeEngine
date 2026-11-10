// do@Redlive

#include "dopch.h"

#include "ShellViewModel.h"

#include "core/application/AssetController.h"
#include "core/application/DocumentController.h"
#include "core/application/EditorViewController.h"
#include "core/application/PlayModeController.h"
#include "core/application/RuntimeController.h"
#include "core/application/ScriptWindowController.h"
#include "core/application/ToolActionController.h"
#include "core/document/EditorDocumentModel.h"

#include <utility>

namespace cakery {

ShellViewModel::ShellViewModel(DocumentController& documents, PlayModeController& play,
                               EditorViewController& view, ToolActionController& tools,
                               ScriptWindowController& scriptWindows, AssetController& assets,
                               RuntimeController& runtime, const String& applicationName)
    : m_documents(documents), m_play(play), m_view(view), m_tools(tools),
      m_scriptWindows(scriptWindows), m_assets(assets), m_runtime(runtime),
      m_applicationName(applicationName)
{
    m_historySubscription = ScopedConnection(
        m_documents.historyChanged,
        m_documents.historyChanged.connect([this]() { refreshHistory(); }));
    m_playSubscription = ScopedConnection(
        m_play.stateChanged,
        m_play.stateChanged.connect([this](PlayState) { refreshRuntime(); }));
    m_cameraSubscription = ScopedConnection(
        m_view.cameraChanged,
        m_view.cameraChanged.connect([this]() { refreshCamera(); }));
    m_gizmoSubscription = ScopedConnection(
        m_view.gizmoChanged,
        m_view.gizmoChanged.connect([this](GizmoMode) { refreshGizmo(); }));
    m_documentSubscription = ScopedConnection(
        m_documents.documentChanged,
        m_documents.documentChanged.connect([this]() { refreshDocument(); }));
    m_missingSubscription = ScopedConnection(
        m_assets.missingAssetReferences,
        m_assets.missingAssetReferences.connect([this](Size_t count) {
            missingAssetReferences.fire(count);
        }));
    refreshHistory();
    refreshRuntime();
    refreshCamera();
    refreshGizmo();
    refreshDocument();
}

ShellViewModel::~ShellViewModel() = default;

String ShellViewModel::windowTitle() const
{
    if (!m_hasDocument) {
        return m_applicationName;
    }
    String title = m_applicationName + " - " + m_sceneName;
    if (m_dirty) {
        title += " *";
    }
    return title;
}

String ShellViewModel::documentPath() const
{
    return m_documents.documentModel().path().string();
}

bool ShellViewModel::playDocumentEdited() const
{
    return m_play.documentEdited();
}

String ShellViewModel::diagnostic() const
{
    return m_runtime.diagnostic();
}

bool ShellViewModel::assetRefreshPending() const
{
    return m_assets.refreshPending();
}

void ShellViewModel::assetRefreshProgress(Size_t& done, Size_t& total) const
{
    m_assets.refreshProgress(done, total);
}

void ShellViewModel::tick()
{
    m_runtime.tick();
}

void ShellViewModel::shutdown()
{
    m_runtime.shutdown();
}

void ShellViewModel::undo()
{
    m_documents.undo();
}

void ShellViewModel::redo()
{
    m_documents.redo();
}

void ShellViewModel::play()
{
    m_play.play();
}

void ShellViewModel::pauseOrResume()
{
    m_play.pauseOrResume();
}

void ShellViewModel::stopPlay(bool keepChanges)
{
    m_play.stop(keepChanges);
}

void ShellViewModel::setCamera2D(bool ortho)
{
    m_view.setCamera2D(ortho);
}

void ShellViewModel::setGizmoMode(GizmoMode mode)
{
    m_view.setGizmoMode(mode);
}

void ShellViewModel::setGizmoSnap(bool enabled)
{
    m_view.setGizmoSnap(enabled);
}

void ShellViewModel::setGizmoSnapStep(float translate, float rotate, float scale)
{
    m_view.setGizmoSnapStep(translate, rotate, scale);
}

bool ShellViewModel::saveDocument(const String& path)
{
    return m_documents.saveDocument(path);
}

bool ShellViewModel::openProject(ProjectDescriptor project)
{
    return m_documents.openProject(std::move(project));
}

bool ShellViewModel::bootEngine()
{
    return m_runtime.bootEngine();
}

bool ShellViewModel::applyRenderSettingsOverlay(const Json& renderSettings)
{
    return m_runtime.applyRenderSettingsOverlay(renderSettings);
}

bool ShellViewModel::listToolActions(DynamicArray<String>& out) const
{
    return m_tools.listToolActions(out);
}

bool ShellViewModel::invokeToolAction(const String& path)
{
    return m_tools.invokeToolAction(path);
}

bool ShellViewModel::listEditorWindows(DynamicArray<Pair<String, String>>& out) const
{
    return m_scriptWindows.listEditorWindows(out);
}

bool ShellViewModel::getEditorWindowUI(const String& id, Json& out) const
{
    return m_scriptWindows.getEditorWindowUI(id, out);
}

bool ShellViewModel::dispatchEditorEvent(const String& owner, const String& ownerId,
                                         const String& controlId, const String& eventName,
                                         const Json& value)
{
    return m_scriptWindows.dispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

void ShellViewModel::refreshHistory()
{
    m_canUndo = m_documents.history().canUndo();
    m_canRedo = m_documents.history().canRedo();
    historyChanged.fire();
}

void ShellViewModel::refreshRuntime()
{
    m_playState = m_play.state();
    runtimeChanged.fire();
}

void ShellViewModel::refreshCamera()
{
    m_is2D = m_view.is2D();
    cameraChanged.fire();
}

void ShellViewModel::refreshGizmo()
{
    m_gizmoMode = m_view.gizmoMode();
    gizmoChanged.fire(m_gizmoMode);
}

void ShellViewModel::refreshDocument()
{
    const EditorDocumentModel& model = m_documents.documentModel();
    m_hasDocument = model.hasDocument();
    m_dirty = model.isDirty();
    m_sceneName = model.name();
    documentChanged.fire();
}

} // namespace cakery
