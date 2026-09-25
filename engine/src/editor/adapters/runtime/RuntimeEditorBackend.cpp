// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "core/EditorSession.h"
#include "adapters/runtime/script/EditorScriptBridge.h"
#include "adapters/runtime/services/FieldAttributes.h"
#include "commands/CreateTilemapWithTilesetCommand.h"
#include "commands/ImportMeshCommand.h"
#include "commands/ImportSpriteCommand.h"
#include "commands/ImportTiledMapCommand.h"
#include "commands/InstantiatePrefabCommand.h"
#include "commands/ReparentEntityCommand.h"
#include "core/document/EditorDocumentSerializer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/project/project.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_view/camera_provider.h"
#include "runtime/function/render/render_view/render_view_manager.h"
#include "runtime/function/render/render_view/render_view_target.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/pixel2d/sprite.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/world/scene.h"
#include "runtime/service/world/scene_importer.h"
#include "runtime/function/world/world.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/resource/asset/importer/import_settings_io.h"
#include "runtime/resource/resource_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

using namespace dodoe;

namespace cakery {

namespace {

BackendLogLevel ToBackendLogLevel(dodoe::LogLevel level)
{
    switch (level) {
    case dodoe::LogLevel::Trace: return BackendLogLevel::Trace;
    case dodoe::LogLevel::Debug: return BackendLogLevel::Debug;
    case dodoe::LogLevel::Info: return BackendLogLevel::Info;
    case dodoe::LogLevel::Warn: return BackendLogLevel::Warning;
    case dodoe::LogLevel::Error: return BackendLogLevel::Error;
    case dodoe::LogLevel::Critical: return BackendLogLevel::Critical;
    }
    return BackendLogLevel::Info;
}

dodoe::ObjectID EnsureSpriteSubObject(const std::string& absolutePath)
{
    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return {};
    }

    const std::filesystem::path abs = std::filesystem::path(absolutePath).lexically_normal();
    const std::string absStr = abs.generic_string();
    const dodoe::ObjectID textureRef = assetManager->ensureImported(dodoe::String(absStr.c_str()));
    if (!textureRef.isValid()) {
        return {};
    }

    const std::filesystem::path metaPath(abs.string() + ".meta");
    dodoe::ImportSettings settings;
    if (dodoe::ImportSettingsIO::Load(dodoe::FsPath(metaPath.string()), settings) &&
        settings.sprites.empty()) {
        auto* texture = resourceManager.loadObjectByPath<dodoe::Texture2D>(
            dodoe::FileID(dodoe::String(absStr.c_str())));
        if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
            dodoe::SpriteMeta sprite;
            sprite.name = dodoe::String("Sprite");
            sprite.local_id = 1;
            sprite.ppu = 10.0f;
            sprite.pivot_x = 0.5f;
            sprite.pivot_y = 0.5f;
            sprite.slice_right = static_cast<dodoe::Float>(texture->getWidth());
            sprite.slice_top = static_cast<dodoe::Float>(texture->getHeight());
            settings.sprites.push_back(sprite);
            dodoe::ImportSettingsIO::Save(dodoe::FsPath(metaPath.string()), settings);
        }
    }

    return assetManager->resolveSubObjectRef(dodoe::FileID(dodoe::String(absStr.c_str())), 0);
}

dodoe::Vector3f ScreenToWorldDropPosition(const EditorCamera* camera, float x, float y)
{
    if (!camera) {
        return {0.0f, 0.0f, 0.0f};
    }
    dodoe::Vector3f origin;
    dodoe::Vector3f dir;
    camera->screenToRay(x, y, origin, dir);
    if (std::abs(dir.z) > 1e-6f) {
        const float t = -origin.z / dir.z;
        if (t >= 0.0f) {
            return origin + dir * t;
        }
    }
    return {origin.x, origin.y, 0.0f};
}

void CollectAssetIds(const nlohmann::json& value, std::vector<std::uint64_t>& out)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "asset_id" && it.value().is_number_unsigned()) {
                const std::uint64_t id = it.value().get<std::uint64_t>();
                if (id != 0) {
                    out.push_back(id);
                }
            } else {
                CollectAssetIds(it.value(), out);
            }
        }
    } else if (value.is_array()) {
        for (const auto& item : value) {
            CollectAssetIds(item, out);
        }
    }
}

} // anonymous namespace

RuntimeEditorBackend::RuntimeEditorBackend()
{
    TilePaintService::RegisterCommands();
    RegisterReparentCommand();
    registerCommandHandlers();
}

RuntimeEditorBackend::~RuntimeEditorBackend()
{
    shutdown();
}

void RuntimeEditorBackend::setEditorSession(EditorSession* session)
{
    m_session = session;
    if (m_session) {
        m_tilePaint = std::make_unique<TilePaintService>(*m_session);
    } else {
        m_tilePaint.reset();
    }
}

dodoe::World* RuntimeEditorBackend::runtimeWorld() const
{
    return m_app ? m_app->context().getWorld() : nullptr;
}

BackendCapabilities RuntimeEditorBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.documentRead = true;
    caps.documentWrite = true;
    caps.scenePreview = true;
    caps.simulation = true;
    return caps;
}

bool RuntimeEditorBackend::inspectComponent(
    const std::string& typeName, std::vector<InspectorFieldMetadata>& fields) const
{
    fields.clear();
    dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(
        dodoe::String(typeName.data(), typeName.size()));
    if (!meta.isValid()) {
        return false;
    }

    FieldAttributeRegistry::self().applyTo(meta, typeName);
    dodoe::FieldAccessor* reflectedFields = nullptr;
    const int count = meta.get_field_list(reflectedFields);
    fields.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        auto& reflected = reflectedFields[i];
        InspectorFieldMetadata field;
        field.name = reflected.getFieldName();
        field.typeName = reflected.getFieldTypeName();
        field.hidden = reflected.isHidden();
        field.readOnly = reflected.isReadOnly();
        field.tooltip = reflected.attribute("Tooltip");
        field.hasRange = reflected.attributeRange(field.rangeMin, field.rangeMax);

        const bool isAssetReference = reflected.hasAttribute("AssetHandle")
            || field.typeName.find("PPtr<") != std::string::npos
            || field.typeName.find("AssetHandle<") != std::string::npos;
        switch (reflected.getFieldType()) {
        case dodoe::FieldType::Bool: field.kind = InspectorFieldKind::Bool; break;
        case dodoe::FieldType::I32: field.kind = InspectorFieldKind::Integer; break;
        case dodoe::FieldType::U32: field.kind = InspectorFieldKind::UnsignedInteger; break;
        case dodoe::FieldType::F32: field.kind = InspectorFieldKind::Float; break;
        case dodoe::FieldType::F64: field.kind = InspectorFieldKind::Double; break;
        case dodoe::FieldType::String: field.kind = InspectorFieldKind::String; break;
        case dodoe::FieldType::Enum: field.kind = InspectorFieldKind::Enum; break;
        case dodoe::FieldType::Vec2:
        case dodoe::FieldType::Vec3:
        case dodoe::FieldType::Vec4:
        case dodoe::FieldType::Vec2i:
        case dodoe::FieldType::Vec3i:
        case dodoe::FieldType::Vec4i: field.kind = InspectorFieldKind::Vector; break;
        case dodoe::FieldType::Color: field.kind = InspectorFieldKind::Color; break;
        case dodoe::FieldType::Struct: field.kind = InspectorFieldKind::Struct; break;
        case dodoe::FieldType::Array: field.kind = InspectorFieldKind::Array; break;
        default: field.kind = InspectorFieldKind::Unknown; break;
        }
        if (isAssetReference) {
            field.kind = InspectorFieldKind::AssetHandle;
        }

        dodoe::EnumValueList enumValues;
        if (reflected.enumValues(enumValues)) {
            field.kind = InspectorFieldKind::Enum;
            field.enumValues.reserve(enumValues.size());
            for (const auto& [name, value] : enumValues) {
                field.enumValues.push_back({name, value});
            }
        }
        fields.push_back(std::move(field));
    }
    delete[] reflectedFields;
    return !fields.empty();
}

bool RuntimeEditorBackend::listAssets(std::vector<AssetBrowserEntry>& entries) const
{
    entries.clear();
    if (!m_assetDatabase) {
        return false;
    }
    for (const auto& asset : m_assetDatabase->list()) {
        AssetBrowserEntry entry;
        entry.uuid = static_cast<std::uint64_t>(asset.uuid);
        entry.path = asset.path;
        entry.name = asset.name;
        entry.type = asset.type;
        entry.extension = asset.extension;
        entry.dirty = asset.dirty;
        entry.dependencies = asset.dependencies;
        entries.push_back(std::move(entry));
    }
    return true;
}

bool RuntimeEditorBackend::findMissingAssetReferences(std::vector<std::uint64_t>& out) const
{
    out.clear();
    if (!m_assetDatabase) {
        return false;
    }
    std::vector<std::uint64_t> ids;
    for (const EditorEntity& entity : m_document.entities) {
        for (const EditorComponent& component : entity.nativeComponents) {
            CollectAssetIds(component.value, ids);
        }
        for (const EditorComponent& component : entity.managedComponents) {
            CollectAssetIds(component.value, ids);
        }
    }
    for (const std::uint64_t id : ids) {
        if (std::find(out.begin(), out.end(), id) != out.end()) {
            continue;
        }
        if (!m_assetDatabase->findByGuid(dodoe::UUID(id)).has_value()) {
            out.push_back(id);
        }
    }
    return true;
}

bool RuntimeEditorBackend::assetRefreshPending() const
{
    return m_assetDatabase && m_assetDatabase->refreshPending();
}

void RuntimeEditorBackend::assetRefreshProgress(std::size_t& done, std::size_t& total) const
{
    done = 0;
    total = 0;
    if (m_assetDatabase) {
        const auto progress = m_assetDatabase->progress();
        done = progress.first;
        total = progress.second;
    }
}

bool RuntimeEditorBackend::getAssetImportSettings(const std::string& path,
                                                  AssetImportSettings& settings) const
{
    settings = AssetImportSettings{};
    auto* assetManager = dodoe::ResourceManager::Self().getAssetManager();
    if (!assetManager || path.empty()) {
        return false;
    }
    dodoe::ImportSettings imported;
    if (!dodoe::ImportSettingsIO::Load(dodoe::FsPath(path), imported)) {
        return false;
    }
    settings.importer = imported.importer.c_str();
    settings.settings = imported.settings;
    return true;
}

bool RuntimeEditorBackend::openProject(const ProjectDescriptor& project)
{
    if (project.rootPath.empty()) {
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: project path is empty.";
        return false;
    }
    m_project = project;

    std::filesystem::path projectFile(project.projectFile);
    if (projectFile.empty()) {
        const std::filesystem::path projectRoot(project.rootPath);
        if (std::filesystem::is_directory(projectRoot)) {
            for (const auto& entry : std::filesystem::directory_iterator(projectRoot)) {
                if (entry.is_regular_file() && entry.path().extension().string() == ".doproj") {
                    projectFile = entry.path();
                    break;
                }
            }
        }
    }
    if (projectFile.empty() || !Project::Load(projectFile)) {
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: project could not be loaded.";
        return false;
    }

    m_state = BackendState::OpeningProject;
    m_diagnostic = "Runtime backend: project loaded, preview boots on surface attach.";
    return true;
}

std::string RuntimeEditorBackend::startScenePath() const
{
    const auto active_project = Project::ActiveProject();
    if (!active_project || active_project->config().start_scene_name.empty()) {
        return {};
    }
    return (Project::AssetDirectory() / "Scenes" /
            (active_project->config().start_scene_name + ".doscn")).string();
}

bool RuntimeEditorBackend::openDocument(const std::string& documentId)
{
    EditorDocument document;
    if (!EditorDocumentSerializer::load(documentId, document)) {
        m_diagnostic = "Runtime backend: scene document could not be parsed.";
        return false;
    }
    m_document = std::move(document);
    m_hasDocument = true;
    if (m_booted && !reconcileScene(m_document)) {
        return false;
    }
    m_diagnostic = "Runtime backend: scene synced from document '" + m_document.name + "'.";
    if (m_booted) {
        reportMissingAssetReferences();
    }
    return true;
}

void RuntimeEditorBackend::registerCommandHandlers()
{
    m_commandHandlers = {
        {"document_changed", &RuntimeEditorBackend::handleDocumentChanged},
        {"scene_mouse_down", &RuntimeEditorBackend::handleSceneMouseDown},
        {"scene_mouse_move", &RuntimeEditorBackend::handleSceneMouseMove},
        {"scene_mouse_up", &RuntimeEditorBackend::handleSceneMouseUp},
        {"scene_mouse_wheel", &RuntimeEditorBackend::handleSceneMouseWheel},
        {"scene_key", &RuntimeEditorBackend::handleSceneKey},
        {"selection_changed", &RuntimeEditorBackend::handleSelectionChanged},
        {"gizmo_mode", &RuntimeEditorBackend::handleGizmoMode},
        {"gizmo_snap", &RuntimeEditorBackend::handleGizmoSnap},
        {"gizmo_snap_step", &RuntimeEditorBackend::handleGizmoSnapStep},
        {"camera_mode", &RuntimeEditorBackend::handleCameraMode},
        {"scene.import_asset", &RuntimeEditorBackend::handleSceneImportAsset},
        {"prefab.export", &RuntimeEditorBackend::handlePrefabExport},
        {"play", &RuntimeEditorBackend::handlePlayAction},
        {"pause", &RuntimeEditorBackend::handlePlayAction},
        {"resume", &RuntimeEditorBackend::handlePlayAction},
        {"stop", &RuntimeEditorBackend::handlePlayAction},
        {"asset.save_all", &RuntimeEditorBackend::handleAssetSaveAll},
        {"asset.refresh", &RuntimeEditorBackend::handleAssetRefresh},
        {"asset.import", &RuntimeEditorBackend::handleAssetImport},
        {"asset.reimport", &RuntimeEditorBackend::handleAssetReimport},
        {"script.tool_action", &RuntimeEditorBackend::handleScriptToolAction},
        {"asset.update_settings", &RuntimeEditorBackend::handleAssetUpdateSettings},
    };
}

bool RuntimeEditorBackend::execute(const EditorCommandMessage& command)
{
    const auto it = m_commandHandlers.find(command.name);
    if (it != m_commandHandlers.end()) {
        return (this->*(it->second))(command);
    }
    if (command.name.rfind("tilemap.", 0) == 0) {
        if (!m_booted || !m_app) {
            return false;
        }
        return executeTilemapCommand(command);
    }
    return false;
}

bool RuntimeEditorBackend::handleDocumentChanged(const EditorCommandMessage& command)
{
    if (command.payload.empty()) {
        return false;
    }
    try {
        const nlohmann::json snapshot = nlohmann::json::parse(command.payload);
        EditorDocument document;
        if (!EditorDocumentSerializer::fromJson(snapshot, document)) {
            return false;
        }
        m_document = std::move(document);
        m_hasDocument = true;
        if (m_booted && !reconcileScene(m_document)) {
            return false;
        }
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    return true;
}

bool RuntimeEditorBackend::handleSceneMouseDown(const EditorCommandMessage& command)
{
    float x = 0.0f, y = 0.0f;
    int button = 0, alt = 0, ctrl = 0, shift = 0;
    if (std::sscanf(command.payload.c_str(), "%f,%f,%d,%d,%d,%d", &x, &y, &button, &alt, &ctrl, &shift) >= 3) {
        m_altHeld = alt != 0;
        m_ctrlHeld = ctrl != 0;
        m_shiftHeld = shift != 0;
        const bool tileEditing = m_tilePaint && m_tilePaint->hasTarget() &&
                                 button == 0 && alt == 0;
        if (tileEditing) {
            int cx = 0, cy = 0;
            if (screenToCell(x, y, cx, cy)) {
                m_tilePaint->setHoverCell(cx, cy);
                m_tilePaint->onCellDown(cx, cy);
            } else {
                m_tilePaint->clearHover();
            }
            m_tilePaintActive = true;
            return true;
        }
        if (button == 0 && alt == 0 && m_selectedUuid != 0 && m_gizmoMode != "none") {
            const int axis = hitTestGizmo(x, y);
            if (axis >= 0) {
                beginDrag(axis, x, y);
                return true;
            }
        }
        if (m_camera) {
            m_camera->onMouseDown(x, y, button, alt != 0);
        }
        if (button == 0 && alt == 0 && m_camera) {
            pickAt(x, y);
        }
    }
    return true;
}

bool RuntimeEditorBackend::handleSceneMouseMove(const EditorCommandMessage& command)
{
    float x = 0.0f, y = 0.0f;
    int ctrl = 0, shift = 0, alt = 0;
    if (std::sscanf(command.payload.c_str(), "%f,%f,%d,%d,%d", &x, &y, &ctrl, &shift, &alt) >= 2) {
        m_ctrlHeld = ctrl != 0;
        m_shiftHeld = shift != 0;
        m_altHeld = alt != 0;
        if (m_tilePaintActive) {
            int cx = 0, cy = 0;
            if (screenToCell(x, y, cx, cy)) {
                m_tilePaint->setHoverCell(cx, cy);
                m_tilePaint->onCellDrag(cx, cy);
            } else {
                m_tilePaint->clearHover();
            }
            return true;
        }
        if (m_tilePaint && m_tilePaint->hasTarget() && m_tilePaint->tool() != TileTool::Select) {
            int cx = 0, cy = 0;
            if (screenToCell(x, y, cx, cy)) {
                m_tilePaint->setHoverCell(cx, cy);
            } else {
                m_tilePaint->clearHover();
            }
        }
        if (m_dragAxis >= 0) {
            updateDrag(x, y);
        } else if (m_camera) {
            m_camera->onMouseMove(x, y);
        }
    }
    return true;
}

bool RuntimeEditorBackend::handleSceneMouseUp(const EditorCommandMessage& command)
{
    int button = 0;
    if (std::sscanf(command.payload.c_str(), "%d", &button) >= 1) {
        if (m_tilePaintActive && button == 0) {
            m_tilePaint->onCellUp();
            m_tilePaintActive = false;
            return true;
        }
        if (m_dragAxis >= 0 && button == 0) {
            endDrag();
        } else if (m_camera) {
            m_camera->onMouseUp(button);
        }
    }
    return true;
}

bool RuntimeEditorBackend::handleSceneMouseWheel(const EditorCommandMessage& command)
{
    float delta = 0.0f;
    if (std::sscanf(command.payload.c_str(), "%f", &delta) >= 1) {
        if (m_camera) {
            m_camera->onScroll(delta);
        }
    }
    return true;
}

bool RuntimeEditorBackend::handleSceneKey(const EditorCommandMessage& command)
{
    int key = 0, down = 0;
    if (std::sscanf(command.payload.c_str(), "%d,%d", &key, &down) >= 2) {
        if (m_camera) {
            m_camera->onKey(key, down != 0);
        }
    }
    return true;
}

bool RuntimeEditorBackend::handleSelectionChanged(const EditorCommandMessage& command)
{
    m_selectedUuid = command.payload.empty()
        ? 0
        : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
    updateTileEditFromSelection();
    return true;
}

bool RuntimeEditorBackend::handleGizmoMode(const EditorCommandMessage& command)
{
    m_gizmoMode = command.payload.empty() ? "none" : command.payload;
    return true;
}

bool RuntimeEditorBackend::handleGizmoSnap(const EditorCommandMessage& command)
{
    if (command.payload.empty()) {
        return false;
    }
    if (command.payload == "1" || command.payload == "true") {
        m_snapEnabled = true;
        return true;
    }
    if (command.payload == "0" || command.payload == "false") {
        m_snapEnabled = false;
        return true;
    }
    try {
        const nlohmann::json payload = nlohmann::json::parse(command.payload);
        if (!payload.is_object()) {
            return false;
        }
        if (payload.contains("enabled")) {
            m_snapEnabled = payload.at("enabled").get<bool>();
        }
        if (payload.contains("translate")) {
            m_translateSnap = std::max(0.0f, payload.at("translate").get<float>());
        }
        if (payload.contains("rotate")) {
            m_rotateSnap = std::max(0.0f, payload.at("rotate").get<float>());
        }
        if (payload.contains("scale")) {
            m_scaleSnap = std::max(0.0f, payload.at("scale").get<float>());
        }
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    return true;
}

bool RuntimeEditorBackend::handleGizmoSnapStep(const EditorCommandMessage& command)
{
    float translate = m_translateSnap, rotate = m_rotateSnap, scale = m_scaleSnap;
    if (std::sscanf(command.payload.c_str(), "%f,%f,%f", &translate, &rotate, &scale) < 1) {
        return false;
    }
    m_translateSnap = std::max(0.0f, translate);
    m_rotateSnap = std::max(0.0f, rotate);
    m_scaleSnap = std::max(0.0f, scale);
    return true;
}

bool RuntimeEditorBackend::handleCameraMode(const EditorCommandMessage& command)
{
    if (!m_camera) {
        return false;
    }
    const bool is2d = command.payload == "2d";
    m_camera->setMode(is2d ? EditorCamera::Mode::Ortho2D : EditorCamera::Mode::Orbit);
    m_eventCallback(BackendEventMessage{"camera_mode_changed", is2d ? "2d" : "3d"});
    return true;
}

bool RuntimeEditorBackend::handleSceneImportAsset(const EditorCommandMessage& command)
{
    if (!m_booted || !m_app || !m_session || !m_camera || command.payload.empty()) {
        return false;
    }
    std::vector<std::string> lines;
    {
        std::istringstream stream(command.payload);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }
    if (lines.size() < 3) {
        return false;
    }
    float dropX = 0.0f;
    float dropY = 0.0f;
    if (std::sscanf(lines[0].c_str(), "%f,%f", &dropX, &dropY) < 2) {
        return false;
    }
    dodoe::Vector3f worldPos = ScreenToWorldDropPosition(m_camera.get(), dropX, dropY);
    nlohmann::json position = {worldPos.x, worldPos.y, worldPos.z};
    bool importedAny = false;
    for (std::size_t i = 1; i + 1 < lines.size(); i += 2) {
        importedAny = importDroppedAsset(lines[i + 1], position) || importedAny;
    }
    return importedAny;
}

bool RuntimeEditorBackend::handlePrefabExport(const EditorCommandMessage& command)
{
    if (!m_booted || !m_app || command.payload.empty()) {
        return false;
    }
    std::istringstream stream(command.payload);
    std::string uuidText;
    std::string path;
    std::getline(stream, uuidText, ',');
    std::getline(stream, path);
    const std::uint64_t uuid = std::strtoull(uuidText.c_str(), nullptr, 10);
    if (uuid == 0 || path.empty()) {
        return false;
    }
    World* world = m_app->context().getWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) {
        return false;
    }
    dodoe::Entity root = scene->tryGetEntityByUUID(dodoe::UUID(uuid));
    if (!root.valid()) {
        return false;
    }
    const dodoe::ObjectID ref = dodoe::SceneImporter::ExportPrefab(dodoe::String(path.c_str()), root);
    if (!ref.isValid()) {
        return false;
    }
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeEditorBackend::handlePlayAction(const EditorCommandMessage& command)
{
    setPlayAction(command.name);
    return true;
}

bool RuntimeEditorBackend::handleAssetSaveAll(const EditorCommandMessage&)
{
    if (m_assetDatabase) {
        m_assetDatabase->saveAllDirty();
    }
    return true;
}

bool RuntimeEditorBackend::handleAssetRefresh(const EditorCommandMessage&)
{
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeEditorBackend::handleAssetImport(const EditorCommandMessage& command)
{
    if (command.payload.empty()) {
        return false;
    }
    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return false;
    }
    const std::filesystem::path source(command.payload);
    const dodoe::ObjectID imported = assetManager->ensureImported(
        dodoe::String(source.is_absolute()
            ? source.lexically_normal().string().c_str()
            : std::filesystem::absolute(source).lexically_normal().string().c_str()));
    if (!imported.isValid()) {
        return false;
    }
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeEditorBackend::handleAssetReimport(const EditorCommandMessage& command)
{
    if (command.payload.empty()) {
        return false;
    }
    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path absolutePath = std::filesystem::absolute(command.payload).lexically_normal();
    const std::filesystem::path relativePath = std::filesystem::relative(
        absolutePath, std::filesystem::path(assetManager->getAssetDir().string()), ec);
    if (ec || relativePath.empty() || relativePath.string().starts_with("..")) {
        return false;
    }
    auto* database = assetManager->getDatabase();
    if (!database) {
        return false;
    }
    dodoe::UUID assetId;
    const std::string normalizedRelative = relativePath.generic_string();
    for (const auto& objectId : database->getAllAssetIDs()) {
        const dodoe::AssetMetaData metadata = database->getMetaData(objectId);
        if (std::filesystem::path(metadata.source_path.c_str()).generic_string() == normalizedRelative) {
            assetId = objectId.asset_id;
            break;
        }
    }
    if (!assetId.isValid() || !assetManager->reimportAsset(assetId)) {
        return false;
    }
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeEditorBackend::handleScriptToolAction(const EditorCommandMessage& command)
{
    return !command.payload.empty() && invokeToolAction(command.payload);
}

bool RuntimeEditorBackend::handleAssetUpdateSettings(const EditorCommandMessage& command)
{
    try {
        const dodoe::Json payload = dodoe::Json::parse(command.payload);
        if (!payload.contains("path") || !payload["path"].is_string() ||
            !payload.contains("settings") || !payload["settings"].is_object()) {
            return false;
        }
        auto* assetManager = dodoe::ResourceManager::Self().getAssetManager();
        if (!assetManager) {
            return false;
        }
        const std::filesystem::path sourcePath = std::filesystem::absolute(
            payload["path"].get<std::string>()).lexically_normal();
        std::error_code ec;
        const std::filesystem::path relativePath = std::filesystem::relative(
            sourcePath, std::filesystem::path(assetManager->getAssetDir().string()), ec);
        if (ec || relativePath.empty() || relativePath.string().starts_with("..")) {
            return false;
        }
        dodoe::ImportSettings importSettings;
        if (!dodoe::ImportSettingsIO::Load(dodoe::FsPath(sourcePath.string()), importSettings)) {
            return false;
        }
        importSettings.settings = payload["settings"];
        if (!dodoe::ImportSettingsIO::Save(dodoe::FsPath(sourcePath.string()), importSettings)) {
            return false;
        }
        if (importSettings.guid.isValid() && !assetManager->reimportAsset(importSettings.guid)) {
            return false;
        }
        if (m_assetDatabase) {
            m_assetDatabase->refresh();
        }
        return true;
    } catch (const dodoe::Json::exception&) {
        return false;
    }
}

bool RuntimeEditorBackend::importDroppedAsset(const std::string& assetPath, const nlohmann::json& position)
{
    const std::filesystem::path absPath = std::filesystem::absolute(assetPath).lexically_normal();
    const std::string absStr = absPath.generic_string();
    const std::string ext = absPath.extension().generic_string();
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string name = absPath.stem().string();
    if (name.empty() || lowerExt.empty()) {
        return false;
    }

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return false;
    }

    if (lowerExt == ".tsx") {
        const dodoe::ObjectID tilesetRef =
            assetManager->ensureTilesetImported(dodoe::String(absStr.c_str()));
        if (!tilesetRef.isValid()) {
            return false;
        }
        auto command2 = std::make_unique<CreateTilemapWithTilesetCommand>(
            dodoe::String(name.c_str()), tilesetRef.asset_id, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const dodoe::UUID created =
                static_cast<CreateTilemapWithTilesetCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<std::uint64_t>(created));
            }
        }
        return true;
    }

    if (lowerExt == ".tmj") {
        const dodoe::ObjectID mapRef =
            assetManager->ensureImported(dodoe::String(absStr.c_str()));
        if (!mapRef.isValid()) {
            return false;
        }
        auto command2 = std::make_unique<ImportTiledMapCommand>(
            dodoe::String(name.c_str()), mapRef.asset_id, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const dodoe::UUID created =
                static_cast<ImportTiledMapCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<std::uint64_t>(created));
            }
        }
        return true;
    }

    if (lowerExt == ".prefab") {
        auto command2 = std::make_unique<InstantiatePrefabCommand>(
            name, absPath, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const dodoe::UUID created =
                static_cast<InstantiatePrefabCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<std::uint64_t>(created));
            }
        }
        return executed != nullptr;
    }

    if (lowerExt == ".doscn") {
        return openDocument(absStr);
    }

    const dodoe::ObjectID imported = assetManager->ensureImported(dodoe::String(absStr.c_str()));
    if (!imported.isValid()) {
        return false;
    }

    if (lowerExt == ".obj" || lowerExt == ".fbx" || lowerExt == ".gltf" || lowerExt == ".glb") {
        nlohmann::json meshValue;
        meshValue["mesh"] = {
            {"asset_id", static_cast<std::uint64_t>(imported.asset_id)},
            {"sub_object_id", 0},
        };
        meshValue["section_index"] = 0;
        meshValue["override_materials"] = nlohmann::json::array();
        meshValue["visible"] = true;
        meshValue["cast_shadow"] = true;
        meshValue["mobility"] = 0;
        auto command2 = std::make_unique<ImportMeshCommand>(name, std::move(meshValue), position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const std::uint64_t created =
                static_cast<ImportMeshCommand*>(executed)->createdUuid();
            if (created != 0) {
                m_session->selection().set(created);
            }
        }
        return true;
    }

    if (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" ||
        lowerExt == ".bmp" || lowerExt == ".gif" || lowerExt == ".tga" ||
        lowerExt == ".psd" || lowerExt == ".hdr") {
        const dodoe::ObjectID spriteRef = EnsureSpriteSubObject(absStr);
        if (!spriteRef.isValid()) {
            return false;
        }
        nlohmann::json spriteValue;
        spriteValue["sprite"] = {
            {"asset_id", static_cast<std::uint64_t>(spriteRef.asset_id)},
            {"sub_object_id", spriteRef.local_id},
        };
        spriteValue["flip"] = false;
        spriteValue["pivot"] = {0.0, 0.0};
        spriteValue["depth"] = 0.0;
        spriteValue["color"] = {1.0, 1.0, 1.0, 1.0};
        auto command2 = std::make_unique<ImportSpriteCommand>(name, std::move(spriteValue), position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const std::uint64_t created =
                static_cast<ImportSpriteCommand*>(executed)->createdUuid();
            if (created != 0) {
                m_session->selection().set(created);
            }
        }
        return true;
    }

    return true;
}

void RuntimeEditorBackend::setEventCallback(std::function<void(const BackendEventMessage&)> callback)
{
    m_eventCallback = std::move(callback);
}

bool RuntimeEditorBackend::attachSceneSurface(const SceneSurfaceDescriptor& surface)
{
    if (surface.nativeHandle == 0) {
        return false;
    }
    m_surface = surface;
    if (m_surface.logicalWidth > 0 && m_surface.logicalHeight > 0 &&
        m_surface.pixelWidth > 0 && m_surface.pixelHeight > 0) {
        m_pending.logicalWidth = m_surface.logicalWidth;
        m_pending.logicalHeight = m_surface.logicalHeight;
        m_pending.devicePixelRatio = m_surface.devicePixelRatio;
        m_pending.pixelWidth = m_surface.pixelWidth;
        m_pending.pixelHeight = m_surface.pixelHeight;
        m_pending.nativeHandle = m_surface.nativeHandle;
        m_hasPendingMetrics = true;
    }
    if (m_booted) {
        applyPendingMetrics();
    }
    return true;
}

bool RuntimeEditorBackend::bootEngine()
{
    if (m_booted) {
        return true;
    }
    if (m_surface.nativeHandle == 0) {
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: engine boot requires an attached scene surface.";
        return false;
    }
    if (!bootRuntime()) {
        return false;
    }
    applyPendingMetrics();
    if (m_hasDocument && !reconcileScene(m_document)) {
        m_diagnostic = "Runtime backend: scene reconcile failed after engine boot.";
        return false;
    }
    m_camera->commitToRenderChannel();
    return true;
}

void RuntimeEditorBackend::requestSceneSurfaceResize(const ViewportMetrics& metrics)
{
    if (metrics.logicalWidth < 1 || metrics.logicalHeight < 1) {
        return;
    }
    m_pending = metrics;
    m_hasPendingMetrics = true;
    if (m_booted) {
        applyPendingMetrics();
    }
}

bool RuntimeEditorBackend::detachSceneSurface()
{
    m_surface = SceneSurfaceDescriptor{};
    return true;
}

void RuntimeEditorBackend::tickAtSafePoint()
{
    if (!m_booted) {
        return;
    }
    DO_PROFILE_SCOPE_CATEGORY("Cakery::tickAtSafePoint", "frame");
    if (m_hasPendingMetrics) {
        applyPendingMetrics();
    }

    if (m_assetDatabase->refreshPending() && m_assetDatabase->refreshFinished()) {
        m_assetDatabase->finalize();
        DO_INFO("Cakery backend: asset database refreshed");
        m_eventCallback(BackendEventMessage{"asset_database_changed", ""});
        reportMissingAssetReferences();
    }

    m_app->stepFrame([this](const float dt) {
        m_camera->update(dt);
        m_camera->commitToRenderChannel();
        updateGizmo();
    });
}

void RuntimeEditorBackend::reportMissingAssetReferences()
{
    if (!m_hasDocument || !m_assetDatabase || m_assetDatabase->refreshPending()) {
        return;
    }
    std::vector<std::uint64_t> missing;
    findMissingAssetReferences(missing);
    const nlohmann::json payload = missing;
    m_eventCallback(BackendEventMessage{"asset_references_missing", payload.dump()});
}

void RuntimeEditorBackend::shutdown()
{
    if (!m_app) {
        return;
    }
    EditorScriptBridge::Shutdown();
    if (m_assetDatabase) {
        m_assetDatabase->cancelAndWait();
    }
    if (m_modulesInitialized) {
        m_app->teardown();
        m_modulesInitialized = false;
    }
    m_camera.reset();
    m_cameraProvider.reset();
    m_sceneTarget = nullptr;
    m_selectedUuid = 0;
    m_booted = false;
    m_app.reset();
    m_state = BackendState::Closed;
    m_diagnostic = "Runtime backend shut down.";
}

BackendStatus RuntimeEditorBackend::status() const
{
    return BackendStatus{ m_state, m_diagnostic };
}

std::string RuntimeEditorBackend::diagnostic() const
{
    return m_diagnostic;
}

bool RuntimeEditorBackend::listLogs(std::vector<BackendLogEntry>& entries) const
{
    entries.clear();
    const auto append = [&entries](const std::vector<dodoe::LogMessage>& logs) {
        for (const dodoe::LogMessage& log : logs) {
            entries.push_back({log.payload, log.logger_name, ToBackendLogLevel(log.level),
                               log.repeat_count, log.sequence});
        }
    };
    append(dodoe::Log::GetCoreLogs());
    append(dodoe::Log::GetClientLogs());
    std::sort(entries.begin(), entries.end(), [](const BackendLogEntry& lhs, const BackendLogEntry& rhs) {
        return lhs.sequence < rhs.sequence;
    });
    return true;
}

bool RuntimeEditorBackend::clearLogs()
{
    dodoe::Log::ClearCoreLogs();
    dodoe::Log::ClearClientLogs();
    return true;
}

bool RuntimeEditorBackend::listToolActions(std::vector<std::string>& actions) const
{
    actions.clear();
    auto* scriptSystem = dodoe::GetScriptSystem();
    if (!scriptSystem) {
        return false;
    }
    dodoe::DynamicArray<dodoe::String> paths;
    if (!scriptSystem->listToolActions(paths)) {
        return false;
    }
    actions.reserve(paths.size());
    for (const dodoe::String& path : paths) {
        actions.push_back(path.c_str());
    }
    return true;
}

bool RuntimeEditorBackend::invokeToolAction(const std::string& path)
{
    auto* scriptSystem = dodoe::GetScriptSystem();
    if (!scriptSystem) {
        return false;
    }
    dodoe::String error;
    if (!scriptSystem->invokeToolAction(dodoe::String(path.c_str()), error)) {
        DO_ERROR("C# Tool Action '{}' failed: {}", path, error.c_str());
        return false;
    }
    return true;
}

bool RuntimeEditorBackend::getCustomInspectorUI(const std::string& typeName, nlohmann::json& out) const
{
    return EditorScriptBridge::GetCustomInspectorUI(typeName, out);
}

bool RuntimeEditorBackend::listEditorWindows(std::vector<std::pair<std::string, std::string>>& out) const
{
    return EditorScriptBridge::ListEditorWindows(out);
}

bool RuntimeEditorBackend::getEditorWindowUI(const std::string& id, nlohmann::json& out) const
{
    return EditorScriptBridge::GetEditorWindowUI(id, out);
}

bool RuntimeEditorBackend::dispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                                               const std::string& controlId, const std::string& eventName,
                                               const nlohmann::json& value)
{
    return EditorScriptBridge::DispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

bool RuntimeEditorBackend::bootRuntime()
{
    if (m_booted) {
        return true;
    }
    DO_PROFILE_SCOPE_CATEGORY("Cakery::bootRuntime", "boot");

    const float bootW = m_pending.logicalWidth > 0 ? static_cast<float>(m_pending.logicalWidth) : 1280.0f;
    const float bootH = m_pending.logicalHeight > 0 ? static_cast<float>(m_pending.logicalHeight) : 720.0f;
    const int bootPixelW = m_pending.pixelWidth > 0 ? m_pending.pixelWidth : static_cast<int>(bootW);
    const int bootPixelH = m_pending.pixelHeight > 0 ? m_pending.pixelHeight : static_cast<int>(bootH);

    ApplicationSpecification spec;
    spec.name = "Cakery";
    spec.app_mode = AppMode::Editor;
    spec.engine_mode = EngineMode::Full;
    spec.window_resizeable = true;
    spec.width = static_cast<UInt32>(bootW);
    spec.height = static_cast<UInt32>(bootH);
    spec.pixel_width = static_cast<UInt32>(bootPixelW);
    spec.pixel_height = static_cast<UInt32>(bootPixelH);
    spec.host_handle = reinterpret_cast<void*>(m_surface.nativeHandle);
    spec.render_settings.api = RenderBackendApiType::D3D12;
    spec.render_settings.pipeline = RenderingPipelineType::Deferred;
    spec.render_settings.create_default_view_target = false;

    m_app = std::make_unique<Application>(spec);
    m_app->startup();
    m_modulesInitialized = true;

    EditorScriptBridge::Initialize(m_session);

    SystemContext& ctx = m_app->context();
    if (!ctx.getRenderSystem()) {
        shutdown();
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: render system missing after startup.";
        return false;
    }
    if (!ctx.getWorld()) {
        shutdown();
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: world missing after startup.";
        return false;
    }
    DO_INFO("Cakery backend: runtime started");

    m_assetDatabase = std::make_unique<AssetDatabase>();
    m_assetDatabase->refresh();
    DO_INFO("Cakery backend: asset database refresh started");

    m_camera = std::make_unique<EditorCamera>();
    m_cameraProvider = std::make_unique<dodoe::EditorCameraProvider>();

    auto* viewMgr = ctx.getRenderSystem()->getViewManager();
    dodoe::RenderViewTargetCreateInfo info;
    info.camera = m_cameraProvider.get();
    info.logical = dodoe::Vector2f(bootW, bootH);
    info.window  = dodoe::Vector2i(static_cast<int>(bootW), static_cast<int>(bootH));
    info.pixel   = dodoe::Vector2i(bootPixelW, bootPixelH);
    m_sceneTarget = viewMgr->createViewTarget(info);
    if (!m_sceneTarget) {
        shutdown();
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: scene view target failed to create.";
        return false;
    }
    DO_INFO("Cakery backend: scene view target created");

    m_camera->setViewportSize(bootW, bootH);

    ctx.getWindowManager()->getWindow()->setPixelSize(bootPixelW, bootPixelH);

    m_booted = true;
    m_state = BackendState::Ready;
    m_diagnostic = "Runtime backend booted.";
    DO_INFO("Cakery backend: runtime boot complete");
    m_eventCallback(BackendEventMessage{"camera_mode_changed", "3d"});
    return true;
}

void RuntimeEditorBackend::applyPendingMetrics()
{
    if (!m_booted || !m_hasPendingMetrics) {
        return;
    }
    SystemContext& ctx = m_app->context();
    auto* window = ctx.getWindowManager()->getWindow();

    const int logicalW = m_pending.logicalWidth;
    const int logicalH = m_pending.logicalHeight;
    const int pixelW = m_pending.pixelWidth;
    const int pixelH = m_pending.pixelHeight;
    window->setSize(logicalW, logicalH);
    window->setPixelSize(pixelW, pixelH);

    m_sceneTarget->setLogicalSize(Vector2f(static_cast<float>(logicalW), static_cast<float>(logicalH)));
    m_sceneTarget->resize(Vector2i(logicalW, logicalH), Vector2i(pixelW, pixelH));
    m_camera->setViewportSize(static_cast<float>(logicalW), static_cast<float>(logicalH));
    m_hasPendingMetrics = false;
}

void RuntimeEditorBackend::setPlayAction(const std::string& action)
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }

    if (action == "play") {
        if (m_playState != "edit") {
            return;
        }
        world->setState(dodoe::WorldState::Runtime);
        m_playState = "playing";
    } else if (action == "pause") {
        if (m_playState != "playing") {
            return;
        }
        world->setState(dodoe::WorldState::Pause);
        m_playState = "paused";
    } else if (action == "resume") {
        if (m_playState != "paused") {
            return;
        }
        world->setState(dodoe::WorldState::Runtime);
        m_playState = "playing";
    } else if (action == "stop") {
        if (m_playState == "edit") {
            return;
        }
        world->setState(dodoe::WorldState::Simulation);
        m_playState = "edit";
    } else {
        return;
    }
    m_eventCallback(BackendEventMessage{"play_state_changed", m_playState});
}

} // namespace cakery
