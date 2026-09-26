// do@Redlive

#include "EditorSession.h"
#include "core/console/CommandRegistry.h"
#include "core/commands/CompositeCommand.h"
#include "core/document/EditorDocumentSerializer.h"

#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cakery {

namespace {

const nlohmann::json* FindJsonPath(const nlohmann::json& root, const std::string& path)
{
    const nlohmann::json* node = &root;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t end = dot == std::string::npos ? path.size() : dot;
        if (!node->is_object()) {
            return nullptr;
        }
        const auto it = node->find(path.substr(start, end - start));
        if (it == node->end()) {
            return nullptr;
        }
        node = &(*it);
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

nlohmann::json* ResolveJsonPath(nlohmann::json& root, const std::string& path)
{
    nlohmann::json* node = &root;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t end = dot == std::string::npos ? path.size() : dot;
        if (!node->is_object()) {
            return nullptr;
        }
        node = &(*node)[path.substr(start, end - start)];
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

} // namespace

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
    m_backend->setEditorSession(this);
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
        const std::uint64_t uuid = m_selection.target() == EditorSelection::Target::Entity
            ? m_selection.selected() : 0;
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
    const std::string startScenePath = m_backend->startScenePath();
    if (!startScenePath.empty()) {
        if (!openDocument(startScenePath)) {
            m_backend->shutdown();
            m_state = EditorSessionState::Failed;
            return false;
        }
    }
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
    if (!m_backend->openDocument(documentPath)) {
        m_documentModel.close();
        m_selection.clear();
        m_history.clear();
        return false;
    }
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

bool EditorSession::inspectComponent(const std::string& typeName,
                                     std::vector<InspectorFieldMetadata>& fields) const
{
    fields.clear();
    return m_backend && m_backend->inspectComponent(typeName, fields);
}

bool EditorSession::listAssets(std::vector<AssetBrowserEntry>& entries) const
{
    entries.clear();
    return m_backend && m_backend->listAssets(entries);
}

bool EditorSession::queryTilemapState(const std::string& tilemapUuid, nlohmann::json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryTilemapState(tilemapUuid, out);
}

bool EditorSession::queryAssetThumbnail(const std::string& path, int size, nlohmann::json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->queryAssetThumbnail(path, size, out);
}

bool EditorSession::getAssetImportSettings(const std::string& path, AssetImportSettings& settings) const
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

std::filesystem::path EditorSession::assetRoot() const
{
    if (m_state != EditorSessionState::Ready && m_state != EditorSessionState::Degraded) {
        return {};
    }
    std::filesystem::path projectFile(m_project.projectFile);
    if (projectFile.empty()) {
        const std::filesystem::path root(m_project.rootPath);
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
    std::filesystem::path projectDir = projectFile.parent_path();
    if (projectDir.empty()) {
        projectDir = m_project.rootPath;
    }
    std::string assetDirName = "Assets";
    if (!projectFile.empty()) {
        std::ifstream file(projectFile);
        if (file.is_open()) {
            try {
                nlohmann::json root;
                file >> root;
                if (root.contains("Project") && root["Project"].is_object() &&
                    root["Project"].contains("AssetDirectory") &&
                    root["Project"]["AssetDirectory"].is_string()) {
                    assetDirName = root["Project"]["AssetDirectory"].get<std::string>();
                }
            } catch (const nlohmann::json::exception&) {
            }
        }
    }
    return projectDir / assetDirName;
}

bool EditorSession::newScene(const std::filesystem::path& directory, const std::string& name)
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
    const std::string sceneName = name.empty() ? std::string("New Scene") : name;
    std::filesystem::path path = directory / (sceneName + ".doscn");
    int counter = 1;
    while (std::filesystem::exists(path)) {
        path = directory / (sceneName + " " + std::to_string(counter) + ".doscn");
        ++counter;
    }
    EditorDocument blank;
    blank.name = sceneName;
    if (!EditorDocumentSerializer::save(blank, path)) {
        return false;
    }
    return openDocument(path.string());
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

bool EditorSession::listLogs(std::vector<BackendLogEntry>& entries) const
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

bool EditorSession::listToolActions(std::vector<std::string>& actions) const
{
    if (!m_backend) {
        actions.clear();
        return false;
    }
    return m_backend->listToolActions(actions);
}

bool EditorSession::invokeToolAction(const std::string& path)
{
    return m_backend && m_backend->invokeToolAction(path);
}

bool EditorSession::getCustomInspectorUI(const std::string& typeName, nlohmann::json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->getCustomInspectorUI(typeName, out);
}

bool EditorSession::listEditorWindows(std::vector<std::pair<std::string, std::string>>& out) const
{
    if (!m_backend) {
        out.clear();
        return false;
    }
    return m_backend->listEditorWindows(out);
}

bool EditorSession::getEditorWindowUI(const std::string& id, nlohmann::json& out) const
{
    if (!m_backend) {
        out = nullptr;
        return false;
    }
    return m_backend->getEditorWindowUI(id, out);
}

bool EditorSession::dispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                                        const std::string& controlId, const std::string& eventName,
                                        const nlohmann::json& value)
{
    return m_backend && m_backend->dispatchEditorEvent(owner, ownerId, controlId, eventName, value);
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
    for (const std::uint64_t selected : m_selection.selectedAll()) {
        if (!m_documentModel.findEntity(selected)) {
            m_selection.remove(selected);
        }
    }
    notifyDocumentChanged();
    return true;
}

bool EditorSession::deleteEntities(const std::vector<std::uint64_t>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    std::vector<std::uint64_t> targets;
    for (const std::uint64_t uuid : uuids) {
        if (!m_documentModel.findEntity(uuid)) {
            continue;
        }
        bool covered = false;
        for (const std::uint64_t other : uuids) {
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
    auto composite = std::make_unique<CompositeCommand>();
    for (const std::uint64_t uuid : targets) {
        composite->addCommand(std::make_unique<DeleteEntityCommand>(uuid));
    }
    if (!m_history.execute(std::move(composite), m_documentModel)) {
        return false;
    }
    for (const std::uint64_t selected : m_selection.selectedAll()) {
        if (!m_documentModel.findEntity(selected)) {
            m_selection.remove(selected);
        }
    }
    notifyDocumentChanged();
    return true;
}

std::vector<EditorEntity> EditorSession::snapshotEntitySubtrees(
    const std::vector<std::uint64_t>& roots) const
{
    std::vector<std::uint64_t> topMost;
    for (const std::uint64_t uuid : roots) {
        if (!m_documentModel.findEntity(uuid)) {
            continue;
        }
        bool covered = false;
        for (const std::uint64_t other : roots) {
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

    std::unordered_set<std::uint64_t> subtree;
    std::vector<std::uint64_t> stack = topMost;
    while (!stack.empty()) {
        const std::uint64_t uuid = stack.back();
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

    std::vector<EditorEntity> snapshot;
    for (const EditorEntity& entity : m_documentModel.entities()) {
        if (subtree.contains(entity.uuid)) {
            snapshot.push_back(entity);
        }
    }
    return snapshot;
}

std::vector<EditorEntity> EditorSession::remapEntityClones(
    const std::vector<EditorEntity>& snapshot, bool keepExternalParent) const
{
    std::unordered_map<std::uint64_t, std::uint64_t> uuidMap;
    uuidMap.reserve(snapshot.size());
    for (const EditorEntity& entity : snapshot) {
        std::uint64_t uuid = m_documentModel.generateEntityUuid();
        while (uuidMap.contains(uuid) || uuid == 0) {
            uuid = m_documentModel.generateEntityUuid();
        }
        uuidMap.emplace(entity.uuid, uuid);
    }

    std::vector<EditorEntity> clones;
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

bool EditorSession::copyEntities(const std::vector<std::uint64_t>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    m_clipboard = snapshotEntitySubtrees(uuids);
    return !m_clipboard.empty();
}

bool EditorSession::cutEntities(const std::vector<std::uint64_t>& uuids)
{
    return copyEntities(uuids) && deleteEntities(uuids);
}

bool EditorSession::pasteEntities()
{
    if (!canEditDocument() || m_clipboard.empty()) {
        return false;
    }
    std::vector<EditorEntity> clones = remapEntityClones(m_clipboard, false);
    std::vector<std::uint64_t> inserted;
    inserted.reserve(clones.size());
    for (const EditorEntity& clone : clones) {
        inserted.push_back(clone.uuid);
    }
    if (!m_history.execute(std::make_unique<InsertEntitiesCommand>(std::move(clones)), m_documentModel)) {
        return false;
    }
    m_selection.selectMany(std::move(inserted));
    notifyDocumentChanged();
    return true;
}

bool EditorSession::duplicateEntities(const std::vector<std::uint64_t>& uuids)
{
    if (!canEditDocument() || uuids.empty()) {
        return false;
    }
    std::vector<EditorEntity> clones = remapEntityClones(snapshotEntitySubtrees(uuids), true);
    if (clones.empty()) {
        return false;
    }
    std::vector<std::uint64_t> inserted;
    inserted.reserve(clones.size());
    for (const EditorEntity& clone : clones) {
        inserted.push_back(clone.uuid);
    }
    if (!m_history.execute(std::make_unique<InsertEntitiesCommand>(std::move(clones)), m_documentModel)) {
        return false;
    }
    m_selection.selectMany(std::move(inserted));
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

bool EditorSession::reparentEntity(std::uint64_t uuid, std::uint64_t newParent)
{
    if (!canEditDocument() || !m_documentModel.findEntity(uuid)) {
        return false;
    }
    if (newParent != 0 && !m_documentModel.findEntity(newParent)) {
        return false;
    }
    auto command = std::make_unique<ReparentDocumentCommand>(uuid, newParent);
    if (!m_history.execute(std::move(command), m_documentModel)) {
        return false;
    }
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

bool EditorSession::moveComponent(std::uint64_t uuid, std::size_t nativeIndex, int delta)
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
            std::make_unique<MoveComponentCommand>(uuid, nativeIndex, static_cast<std::size_t>(target)),
            m_documentModel)) {
        return false;
    }
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

bool EditorSession::updateComponentOnEntities(const std::vector<std::uint64_t>& uuids,
                                              const std::string& typeName,
                                              const nlohmann::json& value,
                                              bool managed)
{
    if (!canEditDocument() || uuids.empty() || typeName.empty()) {
        return false;
    }
    auto composite = std::make_unique<CompositeCommand>();
    for (const std::uint64_t uuid : uuids) {
        const EditorEntity* entity = m_documentModel.findEntity(uuid);
        if (!entity) {
            continue;
        }
        const std::vector<EditorComponent>& components =
            managed ? entity->managedComponents : entity->nativeComponents;
        for (std::size_t i = 0; i < components.size(); ++i) {
            if (components[i].typeName != typeName) {
                continue;
            }
            if (managed) {
                composite->addCommand(std::make_unique<UpdateManagedComponentCommand>(uuid, i, value));
            } else {
                composite->addCommand(std::make_unique<UpdateComponentCommand>(uuid, i, value));
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

bool EditorSession::updateComponentFieldOnEntities(const std::vector<std::uint64_t>& uuids,
                                                   const std::string& typeName,
                                                   const std::string& fieldPath,
                                                   const nlohmann::json& value,
                                                   bool managed)
{
    if (!canEditDocument() || uuids.empty() || typeName.empty() || fieldPath.empty()) {
        return false;
    }
    const nlohmann::json* leaf = FindJsonPath(value, fieldPath);
    if (!leaf) {
        return false;
    }
    auto composite = std::make_unique<CompositeCommand>();
    for (const std::uint64_t uuid : uuids) {
        const EditorEntity* entity = m_documentModel.findEntity(uuid);
        if (!entity) {
            continue;
        }
        const std::vector<EditorComponent>& components =
            managed ? entity->managedComponents : entity->nativeComponents;
        for (std::size_t i = 0; i < components.size(); ++i) {
            if (components[i].typeName != typeName) {
                continue;
            }
            nlohmann::json merged = components[i].value;
            if (nlohmann::json* target = ResolveJsonPath(merged, fieldPath)) {
                *target = *leaf;
                if (managed) {
                    composite->addCommand(std::make_unique<UpdateManagedComponentCommand>(uuid, i, merged));
                } else {
                    composite->addCommand(std::make_unique<UpdateComponentCommand>(uuid, i, merged));
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

bool EditorSession::removeManagedComponent(std::uint64_t uuid, std::size_t index)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    m_history.execute(std::make_unique<RemoveManagedComponentCommand>(uuid, index), m_documentModel);
    notifyDocumentChanged();
    return true;
}

bool EditorSession::updateManagedComponent(std::uint64_t uuid, std::size_t index,
                                            const nlohmann::json& value)
{
    if (!canEditDocument()) {
        return false;
    }
    const EditorEntity* entity = m_documentModel.findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    m_history.execute(std::make_unique<UpdateManagedComponentCommand>(uuid, index, value), m_documentModel);
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
            if (payload.contains("entities") && payload["entities"].is_array()) {
                for (const auto& entry : payload["entities"]) {
                    applyTransformChange(
                        entry.value("uuid", std::uint64_t(0)),
                        entry.value("value", nlohmann::json::object()));
                }
            } else {
                applyTransformChange(
                    payload.value("uuid", std::uint64_t(0)),
                    payload.value("value", nlohmann::json::object()));
            }
            if (!m_transformDragging) {
                notifyDocumentChanged();
            }
        } catch (const nlohmann::json::exception&) {
        }
        return;
    }
    if (event.name == "camera_mode_changed") {
        m_cameraMode = event.payload.empty() ? "3d" : event.payload;
        cameraModeChanged.fire(m_cameraMode);
        return;
    }
    if (event.name == "gizmo_mode_changed") {
        gizmoModeChanged.fire(event.payload.empty() ? std::string("none") : event.payload);
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
        std::size_t count = 0;
        try {
            const nlohmann::json payload = nlohmann::json::parse(event.payload);
            count = payload.is_array() ? payload.size() : payload.get<std::size_t>();
        } catch (const nlohmann::json::exception&) {
            count = 0;
        }
        missingAssetReferencesDetected.fire(count);
        return;
    }
}

bool EditorSession::findMissingAssetReferences(std::vector<std::uint64_t>& out) const
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
    return execute(EditorCommandMessage{"stop", ""});
}

void EditorSession::onPlayStateChanged(const std::string& state)
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

void EditorSession::assetRefreshProgress(std::size_t& done, std::size_t& total) const
{
    m_backend->assetRefreshProgress(done, total);
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
        return;
    }
}

} // namespace cakery
