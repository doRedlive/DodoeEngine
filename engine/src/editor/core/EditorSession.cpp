// do@Redlive

#include "dopch.h"

#include "EditorSession.h"

#include "backend/RuntimeBackend.h"
#include "core/console/AICommandBridge.h"
#include "core/console/CommandRegistry.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/McpServer.h"
#include "core/console/WorkflowTemplate.h"
#include "core/commands/CompositeCommand.h"
#include "core/document/EditorDocumentSerializer.h"
#include "runtime/core/debug/instrumentor.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cakery {

namespace {

const Json* FindJsonPath(const Json& root, const String& path)
{
    const Json* node = &root;
    Size_t start = 0;
    while (start <= path.size()) {
        const Size_t dot = path.find('.', start);
        const Size_t end = dot == String::npos ? path.size() : dot;
        if (!node->is_object()) {
            return nullptr;
        }
        const auto it = node->find(path.substr(start, end - start));
        if (it == node->end()) {
            return nullptr;
        }
        node = &(*it);
        if (dot == String::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

Json* ResolveJsonPath(Json& root, const String& path)
{
    Json* node = &root;
    Size_t start = 0;
    while (start <= path.size()) {
        const Size_t dot = path.find('.', start);
        const Size_t end = dot == String::npos ? path.size() : dot;
        if (!node->is_object()) {
            return nullptr;
        }
        node = &(*node)[path.substr(start, end - start)];
        if (dot == String::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

} // namespace

EditorSession::EditorSession()
    : m_commands(create_scope<CommandRegistry>()),
      m_backend(create_scope<RuntimeBackend>(*this)),
      m_mcp(create_scope<McpServer>()),
      m_recorder(create_scope<ExecutionRecorder>()),
      m_aiBridge(create_scope<AICommandBridge>()),
      m_workflows(create_scope<WorkflowLibrary>())
{
    if (!m_backend) {
        return;
    }
    m_backend->setEventCallback([this](const BackendEventMessage& event) {
        handleBackendEvent(event);
    });

    m_editHistory.changed.connect([this]() {
        notifyDocumentChanged();
    });
    m_playDocumentSubscription = m_documentModel.subscribe([this]() {
        if (m_playState == PlayState::Playing || m_playState == PlayState::Paused) {
            m_playDocumentEdited = true;
        }
    });
    m_selectionSubscription = m_selection.subscribe([this]() {
        if (!m_backend || (m_state != EditorSessionState::Ready &&
                           m_state != EditorSessionState::Degraded)) {
            return;
        }
        const UInt64 uuid = m_selection.target() == EditorSelection::Target::Entity
            ? m_selection.selected() : 0;
        m_backend->execute(SelectionChangedCommand{uuid});
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
    const String startScenePath = m_backend->startScenePath();
    if (!startScenePath.empty()) {
        if (!openDocument(startScenePath)) {
            m_backend->shutdown();
            m_state = EditorSessionState::Failed;
            return false;
        }
    }
    return true;
}

bool EditorSession::openDocument(const String& documentPath)
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
    if (!m_backend->openDocument(documentPath)) {
        m_documentModel.close();
        m_selection.clear();
        m_history.clear();
        return false;
    }
    return true;
}

bool EditorSession::saveDocument(const String& documentPath)
{
    if (!m_documentModel.hasDocument()) {
        return false;
    }
    const String target = documentPath.empty()
        ? String(m_documentModel.path().string().c_str()) : documentPath;
    if (target.empty()) {
        return false;
    }
    return m_documentModel.save(target);
}

bool EditorSession::execute(EditorOperation command)
{
    if (!m_backend || (m_state != EditorSessionState::Ready &&
                       m_state != EditorSessionState::Degraded)) {
        return false;
    }
    return m_backend->execute(command);
}

bool EditorSession::inspectComponent(const String& typeName,
                                     DynamicArray<InspectorFieldMetadata>& fields) const
{
    fields.clear();
    return m_backend && m_backend->inspectComponent(typeName, fields);
}

bool EditorSession::listAssets(DynamicArray<AssetBrowserEntry>& entries) const
{
    entries.clear();
    return m_backend && m_backend->listAssets(entries);
}

bool EditorSession::queryTilemapState(const String& tilemapUuid, Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryTilemapState(tilemapUuid, out);
}

bool EditorSession::queryAssetThumbnail(const String& path, int size, Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryAssetThumbnail(path, size, out);
}

bool EditorSession::queryFrameStats(Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryFrameStats(out);
}

bool EditorSession::queryFps(int& out) const
{
    out = 0;
    if (!m_backend) {
        return false;
    }
    return m_backend->queryFps(out);
}

bool EditorSession::queryRenderGraphSnapshot(Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryRenderGraphSnapshot(out);
}

bool EditorSession::queryRenderSettings(Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryRenderSettings(out);
}

bool EditorSession::getAssetImportSettings(const String& path, AssetImportSettings& settings) const
{
    settings = AssetImportSettings{};
    return m_backend && m_backend->getAssetImportSettings(path, settings);
}

bool EditorSession::attachSceneSurface(SceneSurfaceDescriptor surface)
{
    if (!m_backend || surface.nativeHandle == 0 ||
        (m_state != EditorSessionState::Ready && m_state != EditorSessionState::Degraded)) {
        return false;
    }
    if (surface.logicalWidth > 0 && surface.logicalHeight > 0 &&
        surface.pixelWidth > 0 && surface.pixelHeight > 0) {
        ViewportMetrics metrics;
        metrics.logicalWidth = surface.logicalWidth;
        metrics.logicalHeight = surface.logicalHeight;
        metrics.devicePixelRatio = surface.devicePixelRatio;
        metrics.pixelWidth = surface.pixelWidth;
        metrics.pixelHeight = surface.pixelHeight;
        metrics.nativeHandle = surface.nativeHandle;
        if (metrics.sequence <= m_lastViewportSequence) {
            metrics.sequence = m_lastViewportSequence + 1;
        }
        submitViewportMetrics(metrics);
    }
    m_surfaceAttached = m_backend->attachSceneSurface(surface);
    if (m_surfaceAttached && m_hasPendingViewportMetrics) {
        m_backend->requestSceneSurfaceResize(m_pendingViewportMetrics);
        m_hasPendingViewportMetrics = false;
    }
    return m_surfaceAttached;
}

bool EditorSession::bootEngine()
{
    if (!m_backend || (m_state != EditorSessionState::Ready &&
                       m_state != EditorSessionState::Degraded)) {
        return false;
    }
    return m_backend->bootEngine();
}

bool EditorSession::applyRenderSettingsOverlay(const Json& renderSettings)
{
    if (!m_backend) {
        return false;
    }
    return m_backend->applyRenderSettingsOverlay(renderSettings);
}

bool EditorSession::runBenchmark(int frames, Json& out)
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->runBenchmark(frames, out);
}

bool EditorSession::queryBenchmarkComparison(Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryBenchmarkComparison(out);
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
    DO_PROFILE_SCOPE_CATEGORY("Cakery::sessionTick", "frame");
    if (m_mcp) {
        m_mcp->drain();
    }
    if (m_benchmark.active()) {
        const bool ready = m_backend && (m_state == EditorSessionState::Ready ||
                                         m_state == EditorSessionState::Degraded);
        m_benchmark.tick(ready ? m_backend.get() : nullptr);
        return;
    }
    if (m_backend && (m_state == EditorSessionState::Ready || m_state == EditorSessionState::Degraded)) {
        if (m_surfaceAttached && m_hasPendingViewportMetrics) {
            m_backend->requestSceneSurfaceResize(m_pendingViewportMetrics);
            m_hasPendingViewportMetrics = false;
        }
        m_backend->tickAtSafePoint();
    }
}

bool EditorSession::startBenchmark(int frames, std::function<void(bool, Json)> onFinished,
                                   std::function<void(int, int)> onProgress)
{
    if (!m_backend ||
        (m_state != EditorSessionState::Ready && m_state != EditorSessionState::Degraded)) {
        return false;
    }
    return m_benchmark.start(*m_backend, frames, std::move(onFinished), std::move(onProgress));
}

void EditorSession::shutdown()
{
    if (m_mcp) {
        m_mcp->stop();
    }
    m_benchmark.reset();
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

FsPath EditorSession::assetRoot() const
{
    if (m_state != EditorSessionState::Ready && m_state != EditorSessionState::Degraded) {
        return {};
    }
    FsPath projectFile(m_project.projectFile);
    if (projectFile.empty()) {
        const FsPath root(m_project.rootPath);
        std::error_code ec;
        for (auto it = std::filesystem::directory_iterator(root, ec);
             it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it->is_regular_file(ec) && it->path().extension().string() == ".doproj") {
                projectFile = it->path();
                break;
            }
        }
    }
    FsPath projectDir = projectFile.parent_path();
    if (projectDir.empty()) {
        projectDir = m_project.rootPath;
    }
    String assetDirName = "Assets";
    if (!projectFile.empty()) {
        std::ifstream file(projectFile);
        if (file.is_open()) {
            try {
                Json root;
                file >> root;
                if (root.contains("Project") && root["Project"].is_object() &&
                    root["Project"].contains("AssetDirectory") &&
                    root["Project"]["AssetDirectory"].is_string()) {
                    assetDirName = root["Project"]["AssetDirectory"].get<String>();
                }
            } catch (const Json::exception&) {
            }
        }
    }
    return projectDir / assetDirName;
}

bool EditorSession::newScene(const FsPath& directory, const String& name)
{
    if (m_state != EditorSessionState::Ready && m_state != EditorSessionState::Degraded) {
        return false;
    }
    if (directory.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return false;
    }
    const String sceneName = name.empty() ? String("New Scene") : name;
    FsPath path = directory / (sceneName + ".doscn");
    int counter = 1;
    while (std::filesystem::exists(path)) {
        path = directory / (sceneName + " " + ToString(counter) + ".doscn");
        ++counter;
    }
    EditorDocument blank;
    blank.name = sceneName;
    if (!EditorDocumentSerializer::save(blank, path)) {
        return false;
    }
    return openDocument(path.string().c_str());
}

BackendCapabilities EditorSession::capabilities() const
{
    return m_backend ? m_backend->capabilities() : BackendCapabilities{};
}

BackendStatus EditorSession::status() const
{
    return m_backend ? m_backend->status() : BackendStatus{BackendState::Failed, "No editor backend is available."};
}

String EditorSession::diagnostic() const
{
    return m_backend ? m_backend->diagnostic() : "No editor backend is available.";
}

bool EditorSession::listLogs(DynamicArray<BackendLogEntry>& entries) const
{
    if (!m_backend) {
        entries.clear();
        return false;
    }
    return m_backend->listLogs(entries);
}

bool EditorSession::clearLogs()
{
    return m_backend && m_backend->clearLogs();
}

bool EditorSession::listToolActions(DynamicArray<String>& actions) const
{
    if (!m_backend) {
        actions.clear();
        return false;
    }
    return m_backend->listToolActions(actions);
}

bool EditorSession::invokeToolAction(const String& path)
{
    return m_backend && m_backend->invokeToolAction(path);
}

bool EditorSession::getCustomInspectorUI(const String& typeName, Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->getCustomInspectorUI(typeName, out);
}

bool EditorSession::listEditorWindows(DynamicArray<Pair<String, String>>& out) const
{
    if (!m_backend) {
        out.clear();
        return false;
    }
    return m_backend->listEditorWindows(out);
}

bool EditorSession::getEditorWindowUI(const String& id, Json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->getEditorWindowUI(id, out);
}

bool EditorSession::dispatchEditorEvent(const String& owner, const String& ownerId,
                                        const String& controlId, const String& eventName,
                                        const Json& value)
{
    return m_backend && m_backend->dispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

void EditorSession::attachInspectorConfig(const Json* inspectors)
{
    if (m_backend) {
        m_backend->setInspectorConfig(inspectors);
    }
}

bool EditorSession::canEditDocument() const
{
    return m_documentModel.hasDocument() &&
           (m_state == EditorSessionState::Ready || m_state == EditorSessionState::Degraded);
}

UInt64 EditorSession::createEntity(const String& name)
{
    if (!canEditDocument()) {
        return 0;
    }
    m_history.execute(create_scope<CreateEntityCommand>(name), m_documentModel);
    notifyDocumentChanged();
    const DynamicArray<EditorEntity>& entities = m_documentModel.entities();
    const UInt64 uuid = entities.empty() ? 0 : entities.back().uuid;
    m_selection.set(uuid);
    return uuid;
}

bool EditorSession::deleteEntity(UInt64 uuid)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(create_scope<DeleteEntityCommand>(uuid), m_documentModel);
    for (const UInt64 selected : m_selection.selectedAll()) {
        if (!m_documentModel.findEntity(selected)) {
            m_selection.remove(selected);
        }
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::deleteEntities(const DynamicArray<UInt64>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    DynamicArray<UInt64> targets;
    for (const UInt64 uuid : uuids) {
        if (!m_documentModel.findEntity(uuid)) {
            continue;
        }
        bool covered = false;
        for (const UInt64 other : uuids) {
            if (other != uuid && m_documentModel.isDescendantOf(uuid, other)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            targets.push_back(uuid);
        }
    }
    if (targets.empty()) {
        return false;
    }
    if (targets.size() == 1) {
        return deleteEntity(targets.front());
    }
    auto composite = create_scope<CompositeCommand>();
    for (const UInt64 uuid : targets) {
        composite->addCommand(create_scope<DeleteEntityCommand>(uuid));
    }
    if (!m_history.execute(std::move(composite), m_documentModel)) {
        return false;
    }
    for (const UInt64 selected : m_selection.selectedAll()) {
        if (!m_documentModel.findEntity(selected)) {
            m_selection.remove(selected);
        }
    }
    notifyDocumentChanged();
    return true;
}

DynamicArray<EditorEntity> EditorSession::snapshotEntitySubtrees(
    const DynamicArray<UInt64>& roots) const
{
    DynamicArray<UInt64> topMost;
    for (const UInt64 uuid : roots) {
        if (!m_documentModel.findEntity(uuid)) {
            continue;
        }
        bool covered = false;
        for (const UInt64 other : roots) {
            if (other != uuid && m_documentModel.isDescendantOf(uuid, other)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            topMost.push_back(uuid);
        }
    }
    if (topMost.empty()) {
        return {};
    }

    UnorderedSet<UInt64> subtree;
    DynamicArray<UInt64> stack = topMost;
    while (!stack.empty()) {
        const UInt64 uuid = stack.back();
        stack.pop_back();
        if (!subtree.insert(uuid).second) {
            continue;
        }
        for (const EditorEntity& entity : m_documentModel.entities()) {
            if (entity.parent == uuid) {
                stack.push_back(entity.uuid);
            }
        }
    }

    DynamicArray<EditorEntity> snapshot;
    for (const EditorEntity& entity : m_documentModel.entities()) {
        if (subtree.contains(entity.uuid)) {
            snapshot.push_back(entity);
        }
    }
    return snapshot;
}

DynamicArray<EditorEntity> EditorSession::remapEntityClones(
    const DynamicArray<EditorEntity>& snapshot, bool keepExternalParent) const
{
    UnorderedMap<UInt64, UInt64> uuidMap;
    uuidMap.reserve(snapshot.size());
    for (const EditorEntity& entity : snapshot) {
        UInt64 uuid = m_documentModel.generateEntityUuid();
        while (uuidMap.contains(uuid) || uuid == 0) {
            uuid = m_documentModel.generateEntityUuid();
        }
        uuidMap.emplace(entity.uuid, uuid);
    }

    DynamicArray<EditorEntity> clones;
    clones.reserve(snapshot.size());
    for (const EditorEntity& source : snapshot) {
        EditorEntity clone = source;
        clone.uuid = uuidMap.at(source.uuid);
        const auto parentIt = uuidMap.find(source.parent);
        clone.parent = parentIt != uuidMap.end()
            ? parentIt->second
            : (keepExternalParent ? source.parent : 0);
        for (EditorComponent& component : clone.nativeComponents) {
            if (component.typeName == "IDComponent") {
                component.value["id"] = clone.uuid;
            }
        }
        clones.push_back(std::move(clone));
    }
    return clones;
}

bool EditorSession::copyEntities(const DynamicArray<UInt64>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    m_clipboard = snapshotEntitySubtrees(uuids);
    return !m_clipboard.empty();
}

bool EditorSession::cutEntities(const DynamicArray<UInt64>& uuids)
{
    return copyEntities(uuids) && deleteEntities(uuids);
}

bool EditorSession::pasteEntities()
{
    if (!canEditDocument() || m_clipboard.empty()) {
        return false;
    }
    DynamicArray<EditorEntity> clones = remapEntityClones(m_clipboard, false);
    DynamicArray<UInt64> inserted;
    inserted.reserve(clones.size());
    for (const EditorEntity& clone : clones) {
        inserted.push_back(clone.uuid);
    }
    if (!m_history.execute(create_scope<InsertEntitiesCommand>(std::move(clones)), m_documentModel)) {
        return false;
    }
    m_selection.selectMany(std::move(inserted));
    notifyDocumentChanged();
    return true;
}

bool EditorSession::duplicateEntities(const DynamicArray<UInt64>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    DynamicArray<EditorEntity> clones = remapEntityClones(snapshotEntitySubtrees(uuids), true);
    if (clones.empty()) {
        return false;
    }
    DynamicArray<UInt64> inserted;
    inserted.reserve(clones.size());
    for (const EditorEntity& clone : clones) {
        inserted.push_back(clone.uuid);
    }
    if (!m_history.execute(create_scope<InsertEntitiesCommand>(std::move(clones)), m_documentModel)) {
        return false;
    }
    m_selection.selectMany(std::move(inserted));
    notifyDocumentChanged();
    return true;
}

bool EditorSession::renameEntity(UInt64 uuid, const String& name)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(create_scope<RenameEntityCommand>(uuid, name), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::reparentEntity(UInt64 uuid, UInt64 newParent)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    if (newParent != 0 && !m_documentModel.findEntity(newParent)) {
        return false;
    }
    auto command = create_scope<ReparentDocumentCommand>(uuid, newParent);
    if (!m_history.execute(std::move(command), m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::addComponent(UInt64 uuid, const EditorComponent& component)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    m_history.execute(create_scope<AddComponentCommand>(uuid, component), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::moveComponent(UInt64 uuid, Size_t nativeIndex, int delta)
{
    if (!canEditDocument() || delta == 0) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    const long target = static_cast<long>(nativeIndex) + delta;
    if (target < 0 || target >= static_cast<long>(entity->nativeComponents.size())) {
        return false;
    }
    if (!m_history.execute(
            create_scope<MoveComponentCommand>(uuid, nativeIndex, static_cast<Size_t>(target)),
            m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::removeComponent(UInt64 uuid, Size_t nativeIndex)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    m_history.execute(create_scope<RemoveComponentCommand>(uuid, nativeIndex), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateComponent(UInt64 uuid, Size_t nativeIndex, const Json& value)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    m_history.execute(create_scope<UpdateComponentCommand>(uuid, nativeIndex, value), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateComponentOnEntities(const DynamicArray<UInt64>& uuids,
                                              const String& typeName,
                                              const Json& value,
                                              bool managed)
{
    if (!canEditDocument() || uuids.empty() || typeName.empty()) {
        return false;
    }
    auto composite = create_scope<CompositeCommand>();
    for (const UInt64 uuid : uuids) {
        const EditorEntity* entity = m_documentModel.findEntity(uuid);
        if (!entity) {
            continue;
        }
        const DynamicArray<EditorComponent>& components =
            managed ? entity->managedComponents : entity->nativeComponents;
        for (Size_t i = 0; i < components.size(); ++i) {
            if (components[i].typeName != typeName) {
                continue;
            }
            if (managed) {
                composite->addCommand(create_scope<UpdateManagedComponentCommand>(uuid, i, value));
            } else {
                composite->addCommand(create_scope<UpdateComponentCommand>(uuid, i, value));
            }
            break;
        }
    }
    if (composite->empty() || !m_history.execute(std::move(composite), m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateComponentFieldOnEntities(const DynamicArray<UInt64>& uuids,
                                                   const String& typeName,
                                                   const String& fieldPath,
                                                   const Json& value,
                                                   bool managed)
{
    if (!canEditDocument() || uuids.empty() || typeName.empty() || fieldPath.empty()) {
        return false;
    }
    const Json* leaf = FindJsonPath(value, fieldPath);
    if (!leaf) {
        return false;
    }
    auto composite = create_scope<CompositeCommand>();
    for (const UInt64 uuid : uuids) {
        const EditorEntity* entity = m_documentModel.findEntity(uuid);
        if (!entity) {
            continue;
        }
        const DynamicArray<EditorComponent>& components =
            managed ? entity->managedComponents : entity->nativeComponents;
        for (Size_t i = 0; i < components.size(); ++i) {
            if (components[i].typeName != typeName) {
                continue;
            }
            Json merged = components[i].value;
            if (Json* target = ResolveJsonPath(merged, fieldPath)) {
                *target = *leaf;
                if (managed) {
                    composite->addCommand(create_scope<UpdateManagedComponentCommand>(uuid, i, merged));
                } else {
                    composite->addCommand(create_scope<UpdateComponentCommand>(uuid, i, merged));
                }
            }
            break;
        }
    }
    if (composite->empty() || !m_history.execute(std::move(composite), m_documentModel)) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::removeManagedComponent(UInt64 uuid, Size_t index)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    m_history.execute(create_scope<RemoveManagedComponentCommand>(uuid, index), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateManagedComponent(UInt64 uuid, Size_t index,
                                            const Json& value)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    m_history.execute(create_scope<UpdateManagedComponentCommand>(uuid, index, value), m_documentModel);
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
    m_backend->execute(DocumentChangedCommand{EditorDocumentSerializer::toJson(snapshot)});
}

void EditorSession::handleBackendEvent(const BackendEventMessage& event)
{
    if (event.name == "transform_drag_begin") {
        m_history.beginMerge();
        m_transformDragging = true;
        return;
    }
    if (event.name == "transform_drag_end") {
        m_history.endMerge();
        m_transformDragging = false;
        notifyDocumentChanged();
        return;
    }
    if (event.name == "selection_changed") {
        const UInt64 uuid = event.payload.empty()
            ? 0
            : static_cast<UInt64>(std::strtoull(event.payload.c_str(), nullptr, 10));
        if (m_selection.selected() != uuid) {
            m_selection.set(uuid);
        }
        return;
    }
    if (event.name == "transform_changed") {
        try {
            const Json payload = Json::parse(event.payload);
            if (payload.contains("entities") && payload["entities"].is_array()) {
                for (const auto& entry : payload["entities"]) {
                    applyTransformChange(
                        entry.value("uuid", UInt64(0)),
                        entry.value("value", Json::object()));
                }
            } else {
                applyTransformChange(
                    payload.value("uuid", UInt64(0)),
                    payload.value("value", Json::object()));
            }
            if (!m_transformDragging) {
                notifyDocumentChanged();
            }
        } catch (const Json::exception&) {
        }
        return;
    }
    if (event.name == "camera_mode_changed") {
        m_cameraMode = event.payload.empty() ? "3d" : event.payload;
        cameraModeChanged.fire(m_cameraMode);
        return;
    }
    if (event.name == "gizmo_mode_changed") {
        m_gizmoMode = event.payload.empty() ? String("none") : event.payload;
        gizmoModeChanged.fire(m_gizmoMode);
        return;
    }
    if (event.name == "tilemap_edit_mode") {
        const bool active = event.payload == "1";
        tileEditModeChanged.fire(active);
        return;
    }
    if (event.name == "asset_database_changed") {
        assetDatabaseChanged.fire();
        return;
    }
    if (event.name == "play_state_changed") {
        onPlayStateChanged(event.payload);
        return;
    }
    if (event.name == "asset_references_missing") {
        Size_t count = 0;
        try {
            const Json payload = Json::parse(event.payload);
            count = payload.is_array() ? payload.size() : payload.get<Size_t>();
        } catch (const Json::exception&) {
            count = 0;
        }
        missingAssetReferencesDetected.fire(count);
        return;
    }
}

bool EditorSession::findMissingAssetReferences(DynamicArray<UInt64>& out) const
{
    out.clear();
    return m_backend && m_backend->findMissingAssetReferences(out);
}

bool EditorSession::stopPlay(bool keepPlayChanges)
{
    if (!m_backend) {
        return false;
    }
    if (m_playState == PlayState::Edit) {
        return true;
    }
    m_pendingStopKeep = keepPlayChanges;
    return execute(PlayActionCommand{PlayAction::Stop});
}

void EditorSession::onPlayStateChanged(const String& state)
{
    const PlayState previous = m_playState;
    PlayState next = PlayState::Edit;
    if (state == "playing") {
        next = PlayState::Playing;
    } else if (state == "paused") {
        next = PlayState::Paused;
    }
    m_playState = next;

    if (next == PlayState::Playing && previous == PlayState::Edit) {
        m_hasPlaySnapshot = m_documentModel.hasDocument();
        if (m_hasPlaySnapshot) {
            m_playSnapshot = m_documentModel.document();
            m_playSnapshotDirty = m_documentModel.isDirty();
        }
        m_playDocumentEdited = false;
    }

    if (next == PlayState::Edit && previous != PlayState::Edit && m_hasPlaySnapshot) {
        const bool keepPlayChanges = m_pendingStopKeep;
        m_hasPlaySnapshot = false;
        m_playDocumentEdited = false;
        if (keepPlayChanges) {
            notifyDocumentChanged();
        } else {
            m_documentModel.restoreDocument(m_playSnapshot, m_playSnapshotDirty);
            m_history.clear();
            notifyDocumentChanged();
        }
    }
    m_pendingStopKeep = false;
    playStateChanged.fire(m_playState);
}

bool EditorSession::isAssetRefreshPending() const
{
    return m_backend->assetRefreshPending();
}

void EditorSession::assetRefreshProgress(Size_t& done, Size_t& total) const
{
    m_backend->assetRefreshProgress(done, total);
}

void EditorSession::applyTransformChange(UInt64 uuid, const Json& value)
{
    if (!canEditDocument()) {
        return;
    }
    EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity) {
        return;
    }
    for (Size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName != "TransformComponent") {
            continue;
        }
        m_history.execute(create_scope<UpdateComponentCommand>(uuid, i, value), m_documentModel);
        return;
    }
}

} // namespace cakery
