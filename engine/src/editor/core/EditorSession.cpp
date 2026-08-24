// do@Redlive

#include "EditorSession.h"
#include "core/console/CommandRegistry.h"
#include "core/document/EditorDocumentSerializer.h"

#include <cstdlib>
#include <vector>

namespace cakery {

EditorSession::EditorSession(std::unique_ptr<IEditorBackend> backend)
    : m_backend(std::move(backend))
{
    RegisterBuiltinCommands();
    if (!m_backend) {
        return;
    }
    m_backend->setEventCallback([this](const BackendEventMessage& event) {
        handleBackendEvent(event);
    });
    m_editHistory.changed.connect([this]() {
        notifyDocumentChanged();
    });
    m_selectionSubscription = m_selection.subscribe([this]() {
        if (!m_backend) {
            return;
        }
        const std::uint64_t uuid = m_selection.selected();
        m_backend->execute(EditorCommandMessage{
            "selection_changed",
            uuid == 0 ? std::string() : std::to_string(uuid)
        });
    });
}

EditorSession::~EditorSession()
{
    shutdown();
}

bool EditorSession::openProject(ProjectDescriptor project)
{
    if (!m_backend || project.rootPath.empty()) {
        m_state = EditorSessionState::Failed;
        return false;
    }

    m_state = EditorSessionState::OpeningProject;
    m_project = std::move(project);
    if (!m_backend->openProject(m_project)) {
        m_state = EditorSessionState::Failed;
        return false;
    }

    const BackendCapabilities backendCapabilities = m_backend->capabilities();
    m_state = backendCapabilities.documentRead || backendCapabilities.documentWrite ||
              backendCapabilities.scenePreview || backendCapabilities.simulation
        ? EditorSessionState::Ready
        : EditorSessionState::Degraded;
    return true;
}

bool EditorSession::openDocument(const std::string& documentPath)
{
    if (!m_backend || (m_state != EditorSessionState::Ready &&
                       m_state != EditorSessionState::Degraded)) {
        return false;
    }
    if (!m_documentModel.load(documentPath)) {
        return false;
    }
    m_selection.clear();
    m_history.clear();
    m_backend->openDocument(documentPath);
    return true;
}

bool EditorSession::saveDocument(const std::string& documentPath)
{
    if (!m_documentModel.hasDocument()) {
        return false;
    }
    const std::string target = documentPath.empty() ? m_documentModel.path().string() : documentPath;
    if (target.empty()) {
        return false;
    }
    return m_documentModel.save(target);
}

bool EditorSession::execute(EditorCommandMessage command)
{
    if (!m_backend || (m_state != EditorSessionState::Ready &&
                       m_state != EditorSessionState::Degraded)) {
        return false;
    }
    return m_backend->execute(command);
}

bool EditorSession::attachSceneSurface(SceneSurfaceDescriptor surface)
{
    if (!m_backend || surface.nativeHandle == 0 || m_state == EditorSessionState::Closing ||
        m_state == EditorSessionState::Closed) {
        return false;
    }
    m_surfaceAttached = m_backend->attachSceneSurface(surface);
    if (m_surfaceAttached && m_hasPendingViewportMetrics) {
        m_backend->requestSceneSurfaceResize(m_pendingViewportMetrics);
        m_hasPendingViewportMetrics = false;
    }
    return m_surfaceAttached;
}

void EditorSession::submitViewportMetrics(ViewportMetrics metrics)
{
    if (!m_backend || m_state == EditorSessionState::Closing || m_state == EditorSessionState::Closed) {
        return;
    }
    if (metrics.logicalWidth < 1 || metrics.logicalHeight < 1 ||
        metrics.pixelWidth < 1 || metrics.pixelHeight < 1 ||
        metrics.sequence <= m_lastViewportSequence) {
        return;
    }

    m_lastViewportSequence = metrics.sequence;
    m_pendingViewportMetrics = metrics;
    m_hasPendingViewportMetrics = true;
}

void EditorSession::tick()
{
    if (m_backend && (m_state == EditorSessionState::Ready || m_state == EditorSessionState::Degraded)) {
        if (m_surfaceAttached && m_hasPendingViewportMetrics) {
            m_backend->requestSceneSurfaceResize(m_pendingViewportMetrics);
            m_hasPendingViewportMetrics = false;
        }
        m_backend->tickAtSafePoint();
    }
}

void EditorSession::shutdown()
{
    if (!m_backend || m_state == EditorSessionState::Closed) {
        return;
    }

    m_state = EditorSessionState::Closing;
    m_surfaceAttached = false;
    m_backend->detachSceneSurface();
    m_documentModel.close();
    m_selection.clear();
    m_history.clear();
    m_backend->shutdown();
    m_state = EditorSessionState::Closed;
}

EditorSessionState EditorSession::state() const
{
    return m_state;
}

const ProjectDescriptor& EditorSession::project() const
{
    return m_project;
}

BackendCapabilities EditorSession::capabilities() const
{
    return m_backend ? m_backend->capabilities() : BackendCapabilities{};
}

BackendStatus EditorSession::status() const
{
    return m_backend ? m_backend->status() : BackendStatus{BackendState::Failed, "No editor backend is available."};
}

std::string EditorSession::diagnostic() const
{
    return m_backend ? m_backend->diagnostic() : "No editor backend is available.";
}

bool EditorSession::canEditDocument() const
{
    return m_documentModel.hasDocument() &&
           (m_state == EditorSessionState::Ready || m_state == EditorSessionState::Degraded);
}

std::uint64_t EditorSession::createEntity(const std::string& name)
{
    if (!canEditDocument()) {
        return 0;
    }
    m_history.execute(std::make_unique<CreateEntityCommand>(name), m_documentModel);
    notifyDocumentChanged();
    const std::vector<EditorEntity>& entities = m_documentModel.entities();
    const std::uint64_t uuid = entities.empty() ? 0 : entities.back().uuid;
    m_selection.set(uuid);
    return uuid;
}

bool EditorSession::deleteEntity(std::uint64_t uuid)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(std::make_unique<DeleteEntityCommand>(uuid), m_documentModel);
    if (m_selection.selected() == uuid) {
        m_selection.clear();
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::renameEntity(std::uint64_t uuid, const std::string& name)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(std::make_unique<RenameEntityCommand>(uuid, name), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::addComponent(std::uint64_t uuid, const EditorComponent& component)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(std::make_unique<AddComponentCommand>(uuid, component), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::removeComponent(std::uint64_t uuid, std::size_t nativeIndex)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    m_history.execute(std::make_unique<RemoveComponentCommand>(uuid, nativeIndex), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    m_history.execute(std::make_unique<UpdateComponentCommand>(uuid, nativeIndex, value), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::undo()
{
    if (!canEditDocument() || !m_history.undo(m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::redo()
{
    if (!canEditDocument() || !m_history.redo(m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

void EditorSession::notifyDocumentChanged()
{
    if (!m_backend) {
        return;
    }
    EditorDocument snapshot;
    snapshot.name = m_documentModel.name();
    snapshot.entities = m_documentModel.entities();
    m_backend->execute(EditorCommandMessage{"document_changed", EditorDocumentSerializer::toJson(snapshot).dump()});
}

void EditorSession::handleBackendEvent(const BackendEventMessage& event)
{
    if (event.name == "transform_drag_begin") {
        m_history.beginMerge();
        return;
    }
    if (event.name == "transform_drag_end") {
        m_history.endMerge();
        return;
    }
    if (event.name == "selection_changed") {
        const std::uint64_t uuid = event.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(event.payload.c_str(), nullptr, 10));
        if (m_selection.selected() != uuid) {
            m_selection.set(uuid);
        }
        return;
    }
    if (event.name == "transform_changed") {
        try {
            const nlohmann::json payload = nlohmann::json::parse(event.payload);
            applyTransformChange(
                payload.value("uuid", std::uint64_t(0)),
                payload.value("value", nlohmann::json::object()));
        } catch (const nlohmann::json::exception&) {
        }
    }
}

void EditorSession::applyTransformChange(std::uint64_t uuid, const nlohmann::json& value)
{
    if (!canEditDocument()) {
        return;
    }
    EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity) {
        return;
    }
    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName != "TransformComponent") {
            continue;
        }
        m_history.execute(std::make_unique<UpdateComponentCommand>(uuid, i, value), m_documentModel);
        notifyDocumentChanged();
        return;
    }
}

} // namespace cakery
