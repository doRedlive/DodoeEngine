// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "core/EditorSession.h"
#include "adapters/runtime/services/FieldAttributes.h"
#include "commands/CreateTilemapWithTilesetCommand.h"
#include "commands/ImportMeshCommand.h"
#include "commands/ImportSpriteCommand.h"
#include "commands/ImportTiledMapCommand.h"
#include "commands/ReparentEntityCommand.h"
#include "core/document/EditorDocumentSerializer.h"

#include "runtime/core/application.h"
#include "runtime/core/asserts.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/event/event_system.h"
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

} // anonymous namespace

RuntimeEditorBackend::RuntimeEditorBackend()
{
    TilePaintService::RegisterCommands();
    RegisterReparentCommand();
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

    TaskScheduler::Self();
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
    return true;
}

bool RuntimeEditorBackend::execute(const EditorCommandMessage& command)
{
    if (command.name == "document_changed") {
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

    if (command.name == "scene_mouse_down") {
        float x = 0.0f, y = 0.0f;
        int button = 0, alt = 0;
        if (std::sscanf(command.payload.c_str(), "%f,%f,%d,%d", &x, &y, &button, &alt) >= 3) {
            const bool tilePainting = m_tilePaint && m_tilePaint->hasTarget() &&
                                      m_tilePaint->tool() != TileTool::Select &&
                                      button == 0 && alt == 0;
            if (tilePainting) {
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

    if (command.name == "scene_mouse_move") {
        float x = 0.0f, y = 0.0f;
        if (std::sscanf(command.payload.c_str(), "%f,%f", &x, &y) >= 2) {
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

    if (command.name == "scene_mouse_up") {
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

    if (command.name == "scene_mouse_wheel") {
        float delta = 0.0f;
        if (std::sscanf(command.payload.c_str(), "%f", &delta) >= 1) {
            if (m_camera) {
                m_camera->onScroll(delta);
            }
        }
        return true;
    }

    if (command.name == "scene_key") {
        int key = 0, down = 0;
        if (std::sscanf(command.payload.c_str(), "%d,%d", &key, &down) >= 2) {
            if (m_camera) {
                m_camera->onKey(key, down != 0);
            }
        }
        return true;
    }

    if (command.name == "selection_changed") {
        m_selectedUuid = command.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
        updateTileEditFromSelection();
        return true;
    }

    if (command.name == "gizmo_mode") {
        m_gizmoMode = command.payload.empty() ? "none" : command.payload;
        return true;
    }

    if (command.name == "camera_mode") {
        if (!m_camera) {
            return false;
        }
        const bool is2d = command.payload == "2d";
        m_camera->setMode(is2d ? EditorCamera::Mode::Ortho2D : EditorCamera::Mode::Orbit);
        if (m_eventCallback) {
            m_eventCallback(BackendEventMessage{"camera_mode_changed", is2d ? "2d" : "3d"});
        }
        return true;
    }

    if (command.name == "scene.import_asset") {
        if (!m_booted || !m_app || !m_session || !m_camera || command.payload.empty()) {
            return false;
        }
        std::string line1;
        std::string guidText;
        std::string assetPath;
        {
            std::istringstream stream(command.payload);
            std::getline(stream, line1);
            std::getline(stream, guidText);
            std::getline(stream, assetPath);
        }
        float dropX = 0.0f;
        float dropY = 0.0f;
        if (std::sscanf(line1.c_str(), "%f,%f", &dropX, &dropY) < 2 || assetPath.empty()) {
            return false;
        }
        std::error_code ec;
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

        dodoe::Vector3f worldPos = ScreenToWorldDropPosition(m_camera.get(), dropX, dropY);
        nlohmann::json position = {worldPos.x, worldPos.y, worldPos.z};

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
                dodoe::String(name.c_str()), tilesetRef.asset_id, std::move(position));
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
                dodoe::String(name.c_str()), mapRef.asset_id, std::move(position));
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
            auto command2 = std::make_unique<ImportMeshCommand>(name, std::move(meshValue), std::move(position));
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
            auto command2 = std::make_unique<ImportSpriteCommand>(name, std::move(spriteValue), std::move(position));
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

    if (command.name.rfind("tilemap.", 0) == 0) {
        // Tilemap commands operate on the hosted scene. The editor window can
        // dispatch commands before its scene surface has booted the runtime.
        if (!m_booted || !m_app) {
            return false;
        }
        return executeTilemapCommand(command);
    }

    if (command.name == "play" || command.name == "pause" ||
        command.name == "resume" || command.name == "stop") {
        setPlayAction(command.name);
        return true;
    }

    if (command.name == "asset.save_all") {
        if (m_assetDatabase) {
            m_assetDatabase->saveAllDirty();
        }
        return true;
    }

    if (command.name == "asset.refresh") {
        if (m_assetDatabase) {
            m_assetDatabase->refresh();
        }
        return true;
    }

    if (command.name == "asset.import") {
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

    if (command.name == "asset.reimport") {
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

    if (command.name == "script.tool_action") {
        return !command.payload.empty() && invokeToolAction(command.payload);
    }

    if (command.name == "asset.update_settings") {
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

    return false;
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
    if (!m_booted && !bootRuntime()) {
        return false;
    }
    DO_ASSERT(m_booted, "Cakery backend: runtime did not boot on surface attach");
    applyPendingMetrics();
    if (m_hasDocument && !reconcileScene(m_document)) {
        return false;
    }
    if (m_camera) {
        m_camera->commitToRenderChannel();
    }
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

    if (m_assetDatabase && m_assetDatabase->refreshPending() && m_assetDatabase->refreshFinished()) {
        m_assetDatabase->finalize();
        DO_INFO("Cakery backend: asset database refreshed");
        if (m_eventCallback) {
            m_eventCallback(BackendEventMessage{"asset_database_changed", ""});
        }
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::min(0.05f, std::chrono::duration<float>(now - m_lastTick).count());
    m_lastTick = now;

    EventSystem::Publish<BeforeOneTickEvent>();
    if (auto* input = m_app->context().getInputManager()) {
        input->beginFrame();
    }
    EventSystem::Poll();
    EventSystem::Handle();
    if (auto* input = m_app->context().getInputManager()) {
        input->update(dt);
    }
    if (m_camera) {
        m_camera->update(dt);
        m_camera->commitToRenderChannel();
    }
    updateGizmo();
    m_app->context().tickOneFrame();
    EventSystem::Publish<AfterOneTickEvent>();
}

void RuntimeEditorBackend::shutdown()
{
    if (!m_booted && !m_app) {
        return;
    }
    if (m_assetDatabase) {
        m_assetDatabase->cancelAndWait();
    }
    if (m_booted && m_app) {
        SystemContext& ctx = m_app->context();
        ctx.getLayerStack().detach();
        ctx.stopRuntime();
        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());
        ctx.finalizeModules();
    }
    m_camera.reset();
    m_cameraProvider.reset();
    m_sceneTarget = nullptr;
    m_playSnapshot.reset();
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
    spec.render_settings.threading_mode = ThreadingMode::DualThread;

    m_app = std::make_unique<Application>(spec);
    DO_ASSERT(m_app, "Cakery backend: Application failed to create");
    SystemContext& ctx = m_app->context();

    EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

    ctx.initializeModules();
    DO_ASSERT(ctx.getRenderSystem(), "Cakery backend: render system missing after module init");
    DO_INFO("Cakery backend: runtime modules initialized");
    ctx.startRuntime();
    DO_ASSERT(ctx.getWorld(), "Cakery backend: world missing after startRuntime");
    DO_INFO("Cakery backend: runtime started");
    ctx.getLayerStack().attach();
    DO_INFO("Cakery backend: layer stack attached");

    if (m_assetDatabase) {
        m_assetDatabase->cancelAndWait();
    }
    m_assetDatabase = std::make_unique<AssetDatabase>();
    m_assetDatabase->refresh();
    DO_INFO("Cakery backend: asset database refresh started");

    m_camera = std::make_unique<EditorCamera>();
    m_cameraProvider = std::make_unique<dodoe::EditorCameraProvider>();

    auto* renderSys = ctx.getRenderSystem();
    auto* viewMgr = renderSys ? renderSys->getViewManager() : nullptr;
    if (viewMgr) {
        auto& targets = viewMgr->getTargets();
        if (!targets.empty()) {
            viewMgr->destroyViewTarget(targets[0].get());
        }
        dodoe::RenderViewTargetCreateInfo info;
        info.camera = m_cameraProvider.get();
        info.logical = dodoe::Vector2f(bootW, bootH);
        info.window  = dodoe::Vector2i(static_cast<int>(bootW), static_cast<int>(bootH));
        info.pixel   = dodoe::Vector2i(bootPixelW, bootPixelH);
        m_sceneTarget = viewMgr->createViewTarget(info);
    }
    DO_ASSERT(m_sceneTarget, "Cakery backend: scene view target failed to create");
    DO_INFO("Cakery backend: scene view target created");

    if (m_camera) {
        m_camera->setViewportSize(bootW, bootH);
    }

    if (auto* window = ctx.getWindowManager()->getWindow()) {
        window->setPixelSize(bootPixelW, bootPixelH);
    }

    m_lastTick = std::chrono::steady_clock::now();
    m_booted = true;
    m_state = BackendState::Ready;
    m_diagnostic = "Runtime backend booted.";
    DO_INFO("Cakery backend: runtime boot complete");
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"camera_mode_changed", "3d"});
    }
    return true;
}

void RuntimeEditorBackend::applyPendingMetrics()
{
    if (!m_booted || !m_hasPendingMetrics) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    if (!ctx || !ctx->getWindowManager()) {
        return;
    }
    auto* window = ctx->getWindowManager()->getWindow();
    if (!window) {
        return;
    }

    const int logicalW = m_pending.logicalWidth;
    const int logicalH = m_pending.logicalHeight;
    const int pixelW = m_pending.pixelWidth;
    const int pixelH = m_pending.pixelHeight;
    window->setSize(logicalW, logicalH);
    window->setPixelSize(pixelW, pixelH);

    if (m_sceneTarget) {
        m_sceneTarget->setLogicalSize(Vector2f(static_cast<float>(logicalW), static_cast<float>(logicalH)));
        m_sceneTarget->resize(Vector2i(logicalW, logicalH), Vector2i(pixelW, pixelH));
    }
    if (m_camera) {
        m_camera->setViewportSize(static_cast<float>(logicalW), static_cast<float>(logicalH));
    }
    m_hasPendingMetrics = false;
}

void RuntimeEditorBackend::setPlayAction(const std::string& action)
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }

    if (action == "play") {
        if (m_playState != "edit") {
            return;
        }
        m_playSnapshot = std::make_unique<dodoe::SceneRes>(scene->serialize());
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
        if (m_playSnapshot) {
            scene->deserialize(*m_playSnapshot);
            m_playSnapshot.reset();
        }
        m_playState = "edit";
    }
}

} // namespace cakery
