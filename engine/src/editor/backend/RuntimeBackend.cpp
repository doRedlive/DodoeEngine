// do@Redlive

#include "dopch.h"

#include "RuntimeBackend.h"

#include "EditorCamera.h"
#include "core/EditorSession.h"
#include "backend/script/EditorScriptBridge.h"
#include "services/FieldAttributes.h"
#include "core/commands/CreateTilemapWithTilesetCommand.h"
#include "core/commands/ImportMeshCommand.h"
#include "core/commands/ImportSpriteCommand.h"
#include "core/commands/ImportTiledMapCommand.h"
#include "core/commands/InstantiatePrefabCommand.h"
#include "core/commands/ReparentEntityCommand.h"
#include "core/document/EditorDocumentSerializer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/project/project.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_graph/render_graph_debug.h"
#include "runtime/function/render/render_view/camera_provider.h"
#include "runtime/function/render/render_view/render_view_manager.h"
#include "runtime/function/render/render_view/render_view_target.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
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
#include <thread>

using namespace dodoe;

namespace cakery {

    using dodoe::UUID;

namespace {

BackendLogLevel ToBackendLogLevel(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return BackendLogLevel::Trace;
    case LogLevel::Debug: return BackendLogLevel::Debug;
    case LogLevel::Info: return BackendLogLevel::Info;
    case LogLevel::Warn: return BackendLogLevel::Warning;
    case LogLevel::Error: return BackendLogLevel::Error;
    case LogLevel::Critical: return BackendLogLevel::Critical;
    }
    return BackendLogLevel::Info;
}

ObjectID EnsureSpriteSubObject(const String& absolutePath)
{
    auto& resourceManager = ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();

    const FsPath abs = FsPath(absolutePath).lexically_normal();
    const String absStr(abs.generic_string().c_str());
    const ObjectID textureRef = assetManager->ensureImported(String(absStr));
    if (!textureRef.isValid()) {
        return {};
    }

    const FsPath metaPath(abs.string() + ".meta");
    ImportSettings settings;
    if (ImportSettingsIO::Load(FsPath(metaPath.string()), settings) &&
        settings.sprites.empty()) {
        auto* texture = resourceManager.loadObjectByPath<Texture2D>(
            FileID(String(absStr)));
        if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
            SpriteMeta sprite;
            sprite.name = String("Sprite");
            sprite.local_id = 1;
            sprite.ppu = 10.0f;
            sprite.pivot_x = 0.5f;
            sprite.pivot_y = 0.5f;
            sprite.slice_right = static_cast<Float>(texture->getWidth());
            sprite.slice_top = static_cast<Float>(texture->getHeight());
            settings.sprites.push_back(sprite);
            ImportSettingsIO::Save(FsPath(metaPath.string()), settings);
        }
    }

    return assetManager->resolveSubObjectRef(FileID(String(absStr)), 0);
}

Vector3f ScreenToWorldDropPosition(const EditorCamera* camera, float x, float y)
{
    if (!camera) {
        return {0.0f, 0.0f, 0.0f};
    }
    Vector3f origin;
    Vector3f dir;
    camera->screenToRay(x, y, origin, dir);
    if (std::abs(dir.z) > 1e-6f) {
        const float t = -origin.z / dir.z;
        if (t >= 0.0f) {
            return origin + dir * t;
        }
    }
    return {origin.x, origin.y, 0.0f};
}

void CollectAssetIds(const Json& value, DynamicArray<UInt64>& out)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "asset_id" && it.value().is_number_unsigned()) {
                const UInt64 id = it.value().get<UInt64>();
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

Json SummarizeTelemetry(const DynamicArray<FrameTelemetry>& frames)
{
    Json summary;
    summary["frames"] = frames.size();
    if (frames.empty()) {
        return summary;
    }
    double renderMsSum = 0.0;
    double renderMsMax = 0.0;
    double deltaMsSum = 0.0;
    double gameMsSum = 0.0;
    UInt64 drawCallsSum = 0;
    UInt32 drawCallsMax = 0;
    UInt64 dispatchesSum = 0;
    UInt64 barriersSum = 0;
    UInt64 instancesSum = 0;
    for (const FrameTelemetry& frame : frames) {
        renderMsSum += frame.render_thread_ms;
        renderMsMax = std::max(renderMsMax, static_cast<double>(frame.render_thread_ms));
        gameMsSum += frame.game_thread_ms;
        deltaMsSum += frame.delta_time_ms;
        drawCallsSum += frame.draw_call_count;
        drawCallsMax = std::max(drawCallsMax, frame.draw_call_count);
        dispatchesSum += frame.dispatch_count;
        barriersSum += frame.barrier_count;
        instancesSum += frame.drawn_instance_count;
    }
    const double countF = static_cast<double>(frames.size());
    summary["delta_ms_avg"] = deltaMsSum / countF;
    summary["render_ms_avg"] = renderMsSum / countF;
    summary["render_ms_max"] = renderMsMax;
    summary["game_ms_avg"] = gameMsSum / countF;
    summary["draw_calls_avg"] = static_cast<double>(drawCallsSum) / countF;
    summary["draw_calls_max"] = drawCallsMax;
    summary["dispatches_avg"] = static_cast<double>(dispatchesSum) / countF;
    summary["barriers_avg"] = static_cast<double>(barriersSum) / countF;
    summary["drawn_instances_avg"] = static_cast<double>(instancesSum) / countF;
    return summary;
}

Json TelemetryFramesToJson(const DynamicArray<FrameTelemetry>& frames)
{
    Json framesJson = Json::array();
    for (const FrameTelemetry& frame : frames) {
        const Json parsed = Json::parse(String(frame.toJSON()), nullptr, false);
        if (!parsed.is_discarded()) {
            framesJson.push_back(parsed);
        }
    }
    return framesJson;
}

Json CollectBenchmarkReport(const FrameTelemetryCollector& telemetry,
                                      UInt64 startFrame, UInt64 maxFrameExclusive,
                                      int framesRequested)
{
    DynamicArray<FrameTelemetry> measured;
    const Size_t count = telemetry.getCount();
    for (Size_t i = 0; i < count; ++i) {
        const FrameTelemetry& frame = telemetry.previous(static_cast<UInt32>(i));
        if (frame.frame_number > startFrame && frame.frame_number < maxFrameExclusive) {
            measured.push_back(frame);
        }
    }
    std::reverse(measured.begin(), measured.end());
    Json out = Json::object();
    out["frames_requested"] = framesRequested;
    out["frames_measured"] = measured.size();
    out["summary"] = SummarizeTelemetry(measured);
    out["frames"] = TelemetryFramesToJson(measured);
    return out;
}

} // anonymous namespace

RuntimeBackend::RuntimeBackend(EditorSession& session)
    : m_session(&session)
{
    TilePaintService::RegisterCommands(m_session->commands());
    RegisterReparentCommand(m_session->commands());
    m_tilePaint = create_scope<TilePaintService>(*m_session);
    m_fieldAttributes = create_scope<FieldAttributeRegistry>();
}

RuntimeBackend::~RuntimeBackend()
{
    shutdown();
}

World* RuntimeBackend::runtimeWorld() const
{
    return m_app ? m_app->context().getWorld() : nullptr;
}

BackendCapabilities RuntimeBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.documentRead = true;
    caps.documentWrite = true;
    caps.scenePreview = true;
    caps.simulation = true;
    return caps;
}

void RuntimeBackend::setInspectorConfig(const Json* inspectors)
{
    m_inspectorConfig = inspectors;
}

bool RuntimeBackend::inspectComponent(
    const String& typeName, DynamicArray<InspectorFieldMetadata>& fields) const
{
    fields.clear();
    TypeMeta meta = TypeMeta::newMetaFromName(
        String(typeName.data(), typeName.size()));
    if (!meta.isValid()) {
        return false;
    }

    if (m_fieldAttributes) {
        static const Json emptyInspectors = Json::object();
        m_fieldAttributes->applyTo(meta, typeName,
                                   m_inspectorConfig ? *m_inspectorConfig : emptyInspectors);
    }
    FieldAccessor* reflectedFields = nullptr;
    const int count = meta.get_field_list(reflectedFields);
    fields.reserve(static_cast<Size_t>(count));

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
            || field.typeName.find("PPtr<") != String::npos
            || field.typeName.find("AssetHandle<") != String::npos;
        switch (reflected.getFieldType()) {
        case FieldType::Bool: field.kind = InspectorFieldKind::Bool; break;
        case FieldType::I32: field.kind = InspectorFieldKind::Integer; break;
        case FieldType::U32: field.kind = InspectorFieldKind::UnsignedInteger; break;
        case FieldType::F32: field.kind = InspectorFieldKind::Float; break;
        case FieldType::F64: field.kind = InspectorFieldKind::Double; break;
        case FieldType::String: field.kind = InspectorFieldKind::String; break;
        case FieldType::Enum: field.kind = InspectorFieldKind::Enum; break;
        case FieldType::Vec2:
        case FieldType::Vec3:
        case FieldType::Vec4:
        case FieldType::Vec2i:
        case FieldType::Vec3i:
        case FieldType::Vec4i: field.kind = InspectorFieldKind::Vector; break;
        case FieldType::Color: field.kind = InspectorFieldKind::Color; break;
        case FieldType::Struct: field.kind = InspectorFieldKind::Struct; break;
        case FieldType::Array: field.kind = InspectorFieldKind::Array; break;
        default: field.kind = InspectorFieldKind::Unknown; break;
        }
        if (isAssetReference) {
            field.kind = InspectorFieldKind::AssetHandle;
        }

        EnumValueList enumValues;
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

bool RuntimeBackend::listAssets(DynamicArray<AssetBrowserEntry>& entries) const
{
    entries.clear();
    for (const auto& asset : m_assetDatabase->list()) {
        AssetBrowserEntry entry;
        entry.uuid = static_cast<UInt64>(asset.uuid);
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

bool RuntimeBackend::findMissingAssetReferences(DynamicArray<UInt64>& out) const
{
    out.clear();
    DynamicArray<UInt64> ids;
    for (const EditorEntity& entity : m_document.entities) {
        for (const EditorComponent& component : entity.nativeComponents) {
            CollectAssetIds(component.value, ids);
        }
        for (const EditorComponent& component : entity.managedComponents) {
            CollectAssetIds(component.value, ids);
        }
    }
    for (const UInt64 id : ids) {
        if (std::find(out.begin(), out.end(), id) != out.end()) {
            continue;
        }
        if (!m_assetDatabase->findByGuid(UUID(id)).has_value()) {
            out.push_back(id);
        }
    }
    return true;
}

bool RuntimeBackend::assetRefreshPending() const
{
    return m_assetDatabase && m_assetDatabase->refreshPending();
}

void RuntimeBackend::assetRefreshProgress(Size_t& done, Size_t& total) const
{
    done = 0;
    total = 0;
    if (m_assetDatabase) {
        const auto progress = m_assetDatabase->progress();
        done = progress.first;
        total = progress.second;
    }
}

bool RuntimeBackend::getAssetImportSettings(const String& path,
                                                  AssetImportSettings& settings) const
{
    settings = AssetImportSettings{};
    auto* assetManager = ResourceManager::Self().getAssetManager();
    if (path.empty()) {
        return false;
    }
    ImportSettings imported;
    if (!ImportSettingsIO::Load(FsPath(path), imported)) {
        return false;
    }
    settings.importer = imported.importer;
    settings.settings = imported.settings;
    return true;
}

bool RuntimeBackend::openProject(const ProjectDescriptor& project)
{
    if (project.rootPath.empty()) {
        m_state = BackendState::Failed;
        m_diagnostic = "Runtime backend: project path is empty.";
        return false;
    }
    m_project = project;

    FsPath projectFile(project.projectFile);
    if (projectFile.empty()) {
        const FsPath projectRoot(project.rootPath);
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

String RuntimeBackend::startScenePath() const
{
    const auto active_project = Project::ActiveProject();
    if (!active_project || active_project->config().start_scene_name.empty()) {
        return {};
    }
    return String((Project::AssetDirectory() / "Scenes" /
            (active_project->config().start_scene_name + ".doscn")).string());
}

bool RuntimeBackend::openDocument(const String& documentId)
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
    const String message =
        "Runtime backend: scene synced from document '" + m_document.name + "'.";
    m_diagnostic = message;
    if (m_booted) {
        reportMissingAssetReferences();
    }
    return true;
}

bool RuntimeBackend::execute(const EditorOperation& command)
{
    return std::visit([this](const auto& typedCommand) {
        return handleCommand(typedCommand);
    }, command);
}

bool RuntimeBackend::handleCommand(const DocumentChangedCommand& command)
{
    if (command.snapshot.empty()) {
        return false;
    }
    try {
        EditorDocument document;
        if (!EditorDocumentSerializer::fromJson(command.snapshot, document)) {
            return false;
        }
        m_document = std::move(document);
        m_hasDocument = true;
        if (m_booted && !reconcileScene(m_document)) {
            return false;
        }
    } catch (const Json::exception&) {
        return false;
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SceneMouseDownCommand& command)
{
    m_altHeld = command.alt;
    m_ctrlHeld = command.ctrl;
    m_shiftHeld = command.shift;
    const bool tileEditing = m_tilePaint && m_tilePaint->hasTarget() &&
                             activeTilemapEntity().valid() &&
                             command.button == 0 && !command.alt;
    if (tileEditing) {
        int cx = 0, cy = 0;
        if (screenToCell(command.x, command.y, cx, cy)) {
            m_tilePaint->setHoverCell(cx, cy);
            m_tilePaint->onCellDown(cx, cy);
        } else {
            m_tilePaint->clearHover();
        }
        m_tilePaintActive = true;
        return true;
    }
    if (command.button == 0 && !command.alt && m_selectedUuid != 0 && m_gizmoMode != "none") {
        const int axis = hitTestGizmo(command.x, command.y);
        if (axis >= 0) {
            beginDrag(axis, command.x, command.y);
            return true;
        }
    }
    m_hoverAxis = -1;
    if (m_camera) {
        m_camera->onMouseDown(command.x, command.y, command.button, command.alt);
    }
    if (command.button == 0 && !command.alt && m_camera) {
        requestPick(command.x, command.y);
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SceneMouseMoveCommand& command)
{
    m_ctrlHeld = command.ctrl;
    m_shiftHeld = command.shift;
    m_altHeld = command.alt;
    if (m_tilePaintActive) {
        int cx = 0, cy = 0;
        if (screenToCell(command.x, command.y, cx, cy)) {
            m_tilePaint->setHoverCell(cx, cy);
            m_tilePaint->onCellDrag(cx, cy);
        } else {
            m_tilePaint->clearHover();
        }
        return true;
    }
    if (m_tilePaint && m_tilePaint->hasTarget() && activeTilemapEntity().valid() &&
        m_tilePaint->tool() != TileTool::Select) {
        int cx = 0, cy = 0;
        if (screenToCell(command.x, command.y, cx, cy)) {
            m_tilePaint->setHoverCell(cx, cy);
        } else {
            m_tilePaint->clearHover();
        }
    }
    if (m_dragAxis >= 0) {
        updateDrag(command.x, command.y);
    } else {
        if (m_camera) {
            m_camera->onMouseMove(command.x, command.y);
        }
        m_hoverAxis = (!m_tilePaintActive && m_selectedUuid != 0 && m_gizmoMode != "none")
            ? hitTestGizmo(command.x, command.y) : -1;
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SceneMouseUpCommand& command)
{
    if (m_tilePaintActive && command.button == 0) {
        m_tilePaint->onCellUp();
        m_tilePaintActive = false;
        return true;
    }
    if (m_dragAxis >= 0 && command.button == 0) {
        endDrag();
    } else if (m_camera) {
        m_camera->onMouseUp(command.button);
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SceneMouseWheelCommand& command)
{
    if (m_camera) {
        m_camera->onScroll(command.delta);
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SceneKeyCommand& command)
{
    if (m_camera) {
        m_camera->onKey(command.key, command.down);
    }
    return true;
}

bool RuntimeBackend::handleCommand(const SelectionChangedCommand& command)
{
    m_selectedUuid = command.uuid;
    updateTileEditFromSelection();
    return true;
}

bool RuntimeBackend::handleCommand(const GizmoModeCommand& command)
{
    m_gizmoMode = gizmoModeName(command.mode);
    m_hoverAxis = -1;
    m_eventCallback(BackendEventMessage{"gizmo_mode_changed", m_gizmoMode});
    return true;
}

bool RuntimeBackend::handleCommand(const GizmoSnapCommand& command)
{
    m_snapEnabled = command.enabled;
    return true;
}

bool RuntimeBackend::handleCommand(const GizmoSnapStepCommand& command)
{
    m_translateSnap = std::max(0.0f, command.translate);
    m_rotateSnap = std::max(0.0f, command.rotate);
    m_scaleSnap = std::max(0.0f, command.scale);
    return true;
}

bool RuntimeBackend::handleCommand(const CameraModeCommand& command)
{
    m_camera->setMode(command.ortho2d ? EditorCamera::Mode::Ortho2D : EditorCamera::Mode::Orbit);
    m_eventCallback(BackendEventMessage{"camera_mode_changed", command.ortho2d ? "2d" : "3d"});
    return true;
}

bool RuntimeBackend::handleCommand(const SceneImportAssetCommand& command)
{
    if (command.paths.empty()) {
        return false;
    }
    const Vector3f worldPos = ScreenToWorldDropPosition(m_camera.get(), command.x, command.y);
    Json position = {worldPos.x, worldPos.y, worldPos.z};
    bool importedAny = false;
    for (const String& path : command.paths) {
        importedAny = importDroppedAsset(path, position) || importedAny;
    }
    return importedAny;
}

bool RuntimeBackend::handleCommand(const PrefabExportCommand& command)
{
    if (command.uuid == 0 || command.path.empty()) {
        return false;
    }
    World* world = m_app->context().getWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) {
        return false;
    }
    Entity root = scene->tryGetEntityByUUID(UUID(command.uuid));
    if (!root.valid()) {
        return false;
    }
    const ObjectID ref = SceneImporter::ExportPrefab(command.path, root);
    if (!ref.isValid()) {
        return false;
    }
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeBackend::handleCommand(const PlayActionCommand& command)
{
    setPlayAction(command.action);
    return true;
}

bool RuntimeBackend::handleCommand(const AssetSaveAllCommand&)
{
    if (m_assetDatabase) {
        m_assetDatabase->saveAllDirty();
    }
    return true;
}

bool RuntimeBackend::handleCommand(const AssetRefreshCommand&)
{
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeBackend::handleCommand(const AssetImportCommand& command)
{
    if (command.sourcePath.empty()) {
        return false;
    }
    auto& resourceManager = ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    const FsPath source(command.sourcePath);
    const ObjectID imported = assetManager->ensureImported(
        String(source.is_absolute()
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

bool RuntimeBackend::handleCommand(const AssetReimportCommand& command)
{
    if (command.path.empty()) {
        return false;
    }
    auto& resourceManager = ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    std::error_code ec;
    const FsPath absolutePath = std::filesystem::absolute(command.path).lexically_normal();
    const FsPath relativePath = std::filesystem::relative(
        absolutePath, FsPath(assetManager->getAssetDir().string()), ec);
    if (ec || relativePath.empty() || relativePath.string().starts_with("..")) {
        return false;
    }
    auto* database = assetManager->getDatabase();
    UUID assetId;
    const String normalizedRelative(relativePath.generic_string().c_str());
    for (const auto& objectId : database->getAllAssetIDs()) {
        const AssetMetaData metadata = database->getMetaData(objectId);
        if (String(FsPath(metadata.source_path).generic_string()) == normalizedRelative) {
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

bool RuntimeBackend::handleCommand(const ScriptToolActionCommand& command)
{
    return !command.path.empty() &&
           invokeToolAction(String(command.path.c_str(), command.path.size()));
}

bool RuntimeBackend::handleCommand(const AssetUpdateSettingsCommand& command)
{
    if (command.path.empty() || !command.settings.is_object()) {
        return false;
    }
    auto* assetManager = ResourceManager::Self().getAssetManager();
    const FsPath sourcePath = std::filesystem::absolute(command.path).lexically_normal();
    std::error_code ec;
    const FsPath relativePath = std::filesystem::relative(
        sourcePath, FsPath(assetManager->getAssetDir().string()), ec);
    if (ec || relativePath.empty() || relativePath.string().starts_with("..")) {
        return false;
    }
    ImportSettings importSettings;
    if (!ImportSettingsIO::Load(FsPath(sourcePath.string()), importSettings)) {
        return false;
    }
    importSettings.settings = command.settings;
    if (!ImportSettingsIO::Save(FsPath(sourcePath.string()), importSettings)) {
        return false;
    }
    if (importSettings.guid.isValid() && !assetManager->reimportAsset(importSettings.guid)) {
        return false;
    }
    if (m_assetDatabase) {
        m_assetDatabase->refresh();
    }
    return true;
}

bool RuntimeBackend::importDroppedAsset(const String& assetPath, const Json& position)
{
    const FsPath absPath = std::filesystem::absolute(assetPath).lexically_normal();
    const String absStr(absPath.generic_string().c_str());
    const String ext(absPath.extension().generic_string().c_str());
    String lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                   [](unsigned char c)
{
     return static_cast<char>(std::tolower(c)); });
    const String name = String(absPath.stem().string());
    if (name.empty() || lowerExt.empty()) {
        return false;
    }

    auto& resourceManager = ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();

    if (lowerExt == ".tsx") {
        const ObjectID tilesetRef =
            assetManager->ensureTilesetImported(String(absStr));
        if (!tilesetRef.isValid()) {
            return false;
        }
        auto command2 = create_scope<CreateTilemapWithTilesetCommand>(
            String(name), tilesetRef.asset_id, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const UUID created =
                static_cast<CreateTilemapWithTilesetCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<UInt64>(created));
            }
        }
        return true;
    }

    if (lowerExt == ".tmj") {
        const ObjectID mapRef =
            assetManager->ensureImported(String(absStr));
        if (!mapRef.isValid()) {
            return false;
        }
        auto command2 = create_scope<ImportTiledMapCommand>(
            String(name), mapRef.asset_id, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const UUID created =
                static_cast<ImportTiledMapCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<UInt64>(created));
            }
        }
        return true;
    }

    if (lowerExt == ".prefab") {
        auto command2 = create_scope<InstantiatePrefabCommand>(
            name, absPath, position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const UUID created =
                static_cast<InstantiatePrefabCommand*>(executed)->created();
            if (created.isValid()) {
                m_session->selection().set(static_cast<UInt64>(created));
            }
        }
        return executed != nullptr;
    }

    if (lowerExt == ".doscn") {
        return openDocument(absStr);
    }

    const ObjectID imported = assetManager->ensureImported(String(absStr));
    if (!imported.isValid()) {
        return false;
    }

    if (lowerExt == ".obj" || lowerExt == ".fbx" || lowerExt == ".gltf" || lowerExt == ".glb") {
        Json meshValue;
        meshValue["mesh"] = {
            {"asset_id", static_cast<UInt64>(imported.asset_id)},
            {"sub_object_id", 0},
        };
        meshValue["section_index"] = 0;
        meshValue["override_materials"] = Json::array();
        meshValue["visible"] = true;
        meshValue["cast_shadow"] = true;
        meshValue["mobility"] = 0;
        auto command2 = create_scope<ImportMeshCommand>(name, std::move(meshValue), position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const UInt64 created =
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
        const ObjectID spriteRef = EnsureSpriteSubObject(absStr);
        if (!spriteRef.isValid()) {
            return false;
        }
        Json spriteValue;
        spriteValue["sprite"] = {
            {"asset_id", static_cast<UInt64>(spriteRef.asset_id)},
            {"sub_object_id", spriteRef.local_id},
        };
        spriteValue["flip"] = false;
        spriteValue["pivot"] = {0.0, 0.0};
        spriteValue["depth"] = 0.0;
        spriteValue["color"] = {1.0, 1.0, 1.0, 1.0};
        auto command2 = create_scope<ImportSpriteCommand>(name, std::move(spriteValue), position);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const UInt64 created =
                static_cast<ImportSpriteCommand*>(executed)->createdUuid();
            if (created != 0) {
                m_session->selection().set(created);
            }
        }
        return true;
    }

    return true;
}

void RuntimeBackend::setEventCallback(std::function<void(const BackendEventMessage&)> callback)
{
    m_eventCallback = std::move(callback);
}

bool RuntimeBackend::attachSceneSurface(const SceneSurfaceDescriptor& surface)
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

bool RuntimeBackend::bootEngine()
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

bool RuntimeBackend::applyRenderSettingsOverlay(const Json& renderSettings)
{
    if (!m_booted) {
        return false;
    }
    bool applied = false;
    const auto aaIt = renderSettings.find("aa_mode");
    if (aaIt != renderSettings.end() && aaIt->is_number_integer()) {
        const int aa = aaIt->get<int>();
        if (aa >= 0 && aa <= 3) {
            RenderSettings::SetAntialiasingMode(static_cast<AntialiasingMode>(aa));
            applied = true;
        }
    }
    const auto sampleIt = renderSettings.find("msaa_sample_count");
    if (sampleIt != renderSettings.end() && sampleIt->is_number_integer()) {
        const unsigned int samples = sampleIt->get<unsigned int>();
        if (samples == 1 || samples == 2 || samples == 4 || samples == 8) {
            RenderSettings::SetMsaaSampleCount(static_cast<UInt32>(samples));
            applied = true;
        }
    }
    return applied;
}

bool RuntimeBackend::queryFrameStats(Json& out) const
{
    out = nullptr;
    if (!m_booted || !m_app) {
        return false;
    }
    RenderSystem* renderSystem = m_app->context().getRenderSystem();
    RenderFrameScheduler* scheduler = renderSystem ? renderSystem->getFrameScheduler() : nullptr;
    if (!scheduler) {
        return false;
    }
    const FrameTelemetryCollector& telemetry = scheduler->getTelemetry();
    const Size_t count = telemetry.getCount();
    out = Json::object();
    out["available"] = count > 0;
    out["sample_count"] = count;
    out["history_capacity"] = telemetry.getCapacity();
    if (count == 0) {
        return true;
    }

    DynamicArray<FrameTelemetry> frames;
    frames.reserve(count);
    for (Size_t i = 0; i < count; ++i) {
        frames.push_back(telemetry.previous(static_cast<UInt32>(i)));
    }
    std::reverse(frames.begin(), frames.end());

    out["summary"] = SummarizeTelemetry(frames);
    constexpr Size_t kRecentFrames = 32;
    if (frames.size() > kRecentFrames) {
        frames.erase(frames.begin(), frames.end() - static_cast<std::ptrdiff_t>(kRecentFrames));
    }
    out["frames"] = TelemetryFramesToJson(frames);
    return true;
}

bool RuntimeBackend::queryFps(int& outFps) const
{
    outFps = 0;
    if (!m_booted || !m_app) {
        return false;
    }
    TimeSystem* timeSystem = m_app->context().getTimeSystem();
    if (!timeSystem) {
        return false;
    }
    outFps = timeSystem->getFps();
    return outFps > 0;
}

bool RuntimeBackend::runBenchmark(const int frames, Json& out)
{
    out = nullptr;
    if (!m_booted || !m_app) {
        return false;
    }
    RenderSystem* renderSystem = m_app->context().getRenderSystem();
    RenderFrameScheduler* scheduler = renderSystem ? renderSystem->getFrameScheduler() : nullptr;
    if (!scheduler) {
        return false;
    }
    const FrameTelemetryCollector& telemetry = scheduler->getTelemetry();
    const UInt64 startFrame = telemetry.getCount() > 0 ? telemetry.current().frame_number : 0;

    const int clamped = std::clamp(frames, 16, 3600);
    for (int i = 0; i < clamped; ++i) {
        tickAtSafePoint();
    }

    const UInt64 deadline = TimeSystem::GetSteadyTimeMs() + 5000;
    while (TimeSystem::GetSteadyTimeMs() < deadline) {
        const UInt64 lastFrame = telemetry.getCount() > 0 ? telemetry.current().frame_number : 0;
        if (lastFrame >= startFrame + static_cast<UInt64>(clamped)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    out = CollectBenchmarkReport(telemetry, startFrame, ~0ULL, clamped);

    m_previousBenchmark = std::move(m_lastBenchmark);
    m_lastBenchmark = out;
    return true;
}

bool RuntimeBackend::startBenchmark(const int frames)
{
    if (!m_booted || !m_app) {
        return false;
    }
    RenderSystem* renderSystem = m_app->context().getRenderSystem();
    RenderFrameScheduler* scheduler = renderSystem ? renderSystem->getFrameScheduler() : nullptr;
    if (!scheduler) {
        return false;
    }
    const FrameTelemetryCollector& telemetry = scheduler->getTelemetry();
    m_benchmarkFramesRequested = std::clamp(frames, 16, 3600);
    m_benchmarkStartFrame = telemetry.getCount() > 0 ? telemetry.current().frame_number : 0;
    m_benchmarkDeadlineMs = TimeSystem::GetSteadyTimeMs() + 30000;
    m_benchmarkRunning = true;
    return true;
}

bool RuntimeBackend::pollBenchmark(Json& out)
{
    out = nullptr;
    if (!m_benchmarkRunning || !m_booted || !m_app) {
        return false;
    }
    RenderSystem* renderSystem = m_app->context().getRenderSystem();
    RenderFrameScheduler* scheduler = renderSystem ? renderSystem->getFrameScheduler() : nullptr;
    if (!scheduler) {
        m_benchmarkRunning = false;
        return false;
    }
    const FrameTelemetryCollector& telemetry = scheduler->getTelemetry();
    const UInt64 lastFrame = telemetry.getCount() > 0 ? telemetry.current().frame_number : 0;
    const bool reached = lastFrame >= m_benchmarkStartFrame +
                                            static_cast<UInt64>(m_benchmarkFramesRequested);
    const bool expired = TimeSystem::GetSteadyTimeMs() >= m_benchmarkDeadlineMs;
    if (!reached && !expired) {
        return false;
    }
    m_benchmarkRunning = false;
    out = CollectBenchmarkReport(telemetry, m_benchmarkStartFrame,
                                 m_benchmarkStartFrame +
                                     static_cast<UInt64>(m_benchmarkFramesRequested),
                                 m_benchmarkFramesRequested);

    m_previousBenchmark = std::move(m_lastBenchmark);
    m_lastBenchmark = out;
    return true;
}

bool RuntimeBackend::queryBenchmarkComparison(Json& out) const
{
    out = nullptr;
    if (m_previousBenchmark.is_null() || m_lastBenchmark.is_null()) {
        return false;
    }
    const Json& prev = m_previousBenchmark["summary"];
    const Json& last = m_lastBenchmark["summary"];
    if (!prev.is_object() || !last.is_object()) {
        return false;
    }
    out = Json::object();
    out["baseline_frames"] = m_previousBenchmark.value("frames_measured", 0);
    out["current_frames"] = m_lastBenchmark.value("frames_measured", 0);

    Json deltas = Json::object();
    for (auto it = prev.begin(); it != prev.end(); ++it) {
        const auto rit = last.find(it.key());
        if (rit == last.end() || !it->is_number() || !rit->is_number()) {
            continue;
        }
        const double before = it->get<double>();
        const double after = rit->get<double>();
        Json delta;
        delta["before"] = before;
        delta["after"] = after;
        delta["delta"] = after - before;
        delta["delta_pct"] = before != 0.0 ? ((after - before) / before) * 100.0 : 0.0;
        deltas[it.key()] = std::move(delta);
    }
    out["deltas"] = std::move(deltas);
    return true;
}

bool RuntimeBackend::queryRenderGraphSnapshot(Json& out) const
{
    out = nullptr;
#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
    if (!m_booted || !m_app) {
        return false;
    }
    const Ref<RenderGraphDebugSnapshot> snapshot = RenderGraphDebug::Snapshot();
    if (!snapshot) {
        return false;
    }
    out = Json::object();
    out["sequence"] = snapshot->sequence;
    out["pass_count"] = snapshot->pass_count;
    out["culled_count"] = snapshot->culled_count;
    out["resource_count"] = snapshot->resource_count;
    out["level_count"] = snapshot->level_count;

    Json subgraphs = Json::array();
    for (const String& name : snapshot->subgraph_names) {
        subgraphs.push_back(String(name));
    }
    out["subgraph_names"] = std::move(subgraphs);

    Json passes = Json::array();
    for (const RenderGraphDebugPass& pass : snapshot->passes) {
        Json item;
        item["name"] = String(pass.name);
        item["culled"] = pass.culled;
        item["level"] = pass.level;
        item["barrier_count"] = pass.barrier_count;
        item["cpu_ms"] = pass.cpu_ms;
        item["subgraph_index"] = pass.subgraph_index;
        const UInt32 flags = static_cast<UInt32>(pass.flags);
        item["raster"] = (flags & static_cast<UInt32>(RenderGraphPassFlags::Raster)) != 0;
        item["compute"] = (flags & static_cast<UInt32>(RenderGraphPassFlags::Compute)) != 0;
        item["copy"] = (flags & static_cast<UInt32>(RenderGraphPassFlags::Copy)) != 0;
        item["async_compute"] = (flags & static_cast<UInt32>(RenderGraphPassFlags::AsyncCompute)) != 0;
        item["never_cull"] = (flags & static_cast<UInt32>(RenderGraphPassFlags::NeverCull)) != 0;
        passes.push_back(std::move(item));
    }
    out["passes"] = std::move(passes);

    Json levels = Json::array();
    for (const DynamicArray<UInt32>& level : snapshot->levels) {
        Json indices = Json::array();
        for (const UInt32 passIndex : level) {
            indices.push_back(passIndex);
        }
        levels.push_back(std::move(indices));
    }
    out["levels"] = std::move(levels);
    return true;
#else
    return false;
#endif
}

bool RuntimeBackend::queryRenderSettings(Json& out) const
{
    out = nullptr;
    if (!m_booted || !m_app) {
        return false;
    }

    out = Json::object();
    out["api"] = static_cast<int>(RenderSettings::GetRenderBackendApiType());
    out["api_name"] = String(RenderSettings::GetRenderBackendApiTypeStr());
    out["pipeline"] = static_cast<int>(RenderSettings::GetRenderingPipelineType());
    out["present_mode"] = static_cast<int>(RenderSettings::GetPresentMode());
    out["aa_mode"] = static_cast<int>(RenderSettings::GetAntialiasingMode());
    out["msaa_sample_count"] = RenderSettings::GetMsaaSampleCountSetting();
    out["msaa_effective_sample_count"] = RenderSettings::GetMsaaSampleCount();
    out["msaa_enabled"] = RenderSettings::IsMsaaEnabled();
    out["fxaa_enabled"] = RenderSettings::IsFxaaEnabled();
    out["taa_enabled"] = RenderSettings::IsTaaEnabled();
    out["enable_single_thread"] = RenderSettings::IsSingleThread();
    out["baseline_renderer"] = RenderSettings::IsEnableBaselineRender();
    out["windowless"] = RenderSettings::IsWindowless();

    Json enums;
    enums["api"] = {"None", "OpenGL", "Vulkan", "D3D12"};
    enums["pipeline"] = {"None", "Forward", "ForwardPlus", "Deferred", "DeferredPlus", "Only2D", "OnlyGUI"};
    enums["present_mode"] = {"VSync", "Mailbox", "Immediate"};
    enums["aa_mode"] = {"None", "MSAA", "FXAA", "TAA"};
    out["enum_names"] = std::move(enums);

    Json resolved;
    const ResolvedRenderFeatures& features = RenderSettings::GetResolvedFeatures();
    resolved["gpu_driven_active"] = features.gpu_driven_active;
    resolved["async_compute_active"] = features.async_compute_active;
    resolved["bindless_active"] = features.bindless_active;
    resolved["gpu_driven_fallback_reason"] = String(features.gpu_driven_fallback_reason);
    out["resolved_features"] = std::move(resolved);

    Json viewport;
    viewport["attached"] = m_surface.logicalWidth > 0;
    viewport["logical_width"] = m_surface.logicalWidth;
    viewport["logical_height"] = m_surface.logicalHeight;
    viewport["pixel_width"] = m_surface.pixelWidth;
    viewport["pixel_height"] = m_surface.pixelHeight;
    viewport["device_pixel_ratio"] = m_surface.devicePixelRatio;
    out["viewport"] = std::move(viewport);
    return true;
}

void RuntimeBackend::requestSceneSurfaceResize(const ViewportMetrics& metrics)
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

bool RuntimeBackend::detachSceneSurface()
{
    m_surface = SceneSurfaceDescriptor{};
    return true;
}

void RuntimeBackend::tickAtSafePoint()
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

    auto& pick_channel = GetPickChannel().get<PickChannelData>();
    if (pick_channel.result.handled && m_pick_sequence != 0 &&
        pick_channel.result.sequence == m_pick_sequence) {
        pick_channel.result.handled = false;
        setSelectedUuid(pick_channel.result.entity_uuid);
    }
}

void RuntimeBackend::reportMissingAssetReferences()
{
    if (!m_hasDocument || !m_assetDatabase || m_assetDatabase->refreshPending()) {
        return;
    }
    DynamicArray<UInt64> missing;
    findMissingAssetReferences(missing);
    const Json payload = missing;
    m_eventCallback(BackendEventMessage{"asset_references_missing", ToString(payload)});
}

void RuntimeBackend::shutdown()
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

BackendStatus RuntimeBackend::status() const
{
    return BackendStatus{ m_state, m_diagnostic };
}

String RuntimeBackend::diagnostic() const
{
    return m_diagnostic;
}

bool RuntimeBackend::listLogs(DynamicArray<BackendLogEntry>& entries) const
{
    entries.clear();
    const auto append = [&entries](const auto& logs) {
        for (const LogMessage& log : logs) {
            entries.push_back({String(log.payload), String(log.logger_name),
                               ToBackendLogLevel(log.level), log.repeat_count, log.sequence});
        }
    };
    append(Log::GetCoreLogs());
    append(Log::GetClientLogs());
    std::sort(entries.begin(), entries.end(), [](const BackendLogEntry& lhs, const BackendLogEntry& rhs) {
        return lhs.sequence < rhs.sequence;
    });
    return true;
}

bool RuntimeBackend::clearLogs()
{
    Log::ClearCoreLogs();
    Log::ClearClientLogs();
    return true;
}

bool RuntimeBackend::listToolActions(DynamicArray<String>& actions) const
{
    actions.clear();
    auto* scriptSystem = GetScriptSystem();
    if (!scriptSystem) {
        return false;
    }
    DynamicArray<String> paths;
    if (!scriptSystem->listToolActions(paths)) {
        return false;
    }
    actions.reserve(paths.size());
    for (const String& path : paths) {
        actions.push_back(path);
    }
    return true;
}

bool RuntimeBackend::invokeToolAction(const String& path)
{
    auto* scriptSystem = GetScriptSystem();
    if (!scriptSystem) {
        return false;
    }
    String error;
    if (!scriptSystem->invokeToolAction(String(path), error)) {
        DO_ERROR("C# Tool Action '{}' failed: {}", path, error);
        return false;
    }
    return true;
}

bool RuntimeBackend::getCustomInspectorUI(const String& typeName, Json& out) const
{
    return EditorScriptBridge::GetCustomInspectorUI(typeName, out);
}

bool RuntimeBackend::listEditorWindows(DynamicArray<Pair<String, String>>& out) const
{
    return EditorScriptBridge::ListEditorWindows(out);
}

bool RuntimeBackend::getEditorWindowUI(const String& id, Json& out) const
{
    return EditorScriptBridge::GetEditorWindowUI(id, out);
}

bool RuntimeBackend::dispatchEditorEvent(const String& owner, const String& ownerId,
                                               const String& controlId, const String& eventName,
                                               const Json& value)
{
    return EditorScriptBridge::DispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

bool RuntimeBackend::bootRuntime()
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

    m_app = create_scope<Application>(spec);
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

    m_assetDatabase = create_scope<AssetDatabase>();
    m_assetDatabase->refresh();
    DO_INFO("Cakery backend: asset database refresh started");

    m_camera = create_scope<EditorCamera>();
    m_cameraProvider = create_scope<EditorCameraProvider>();

    auto* viewMgr = ctx.getRenderSystem()->getViewManager();
    RenderViewTargetCreateInfo info;
    info.camera = m_cameraProvider.get();
    info.logical = Vector2f(bootW, bootH);
    info.window  = Vector2i(static_cast<int>(bootW), static_cast<int>(bootH));
    info.pixel   = Vector2i(bootPixelW, bootPixelH);
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
    m_eventCallback(BackendEventMessage{"gizmo_mode_changed", m_gizmoMode});
    return true;
}

void RuntimeBackend::applyPendingMetrics()
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

void RuntimeBackend::setPlayAction(PlayAction action)
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }

    switch (action) {
    case PlayAction::Play:
        if (m_playState != "edit") {
            return;
        }
        world->setState(WorldState::Runtime);
        m_playState = "playing";
        break;
    case PlayAction::Pause:
        if (m_playState != "playing") {
            return;
        }
        world->setState(WorldState::Pause);
        m_playState = "paused";
        break;
    case PlayAction::Resume:
        if (m_playState != "paused") {
            return;
        }
        world->setState(WorldState::Runtime);
        m_playState = "playing";
        break;
    case PlayAction::Stop:
        if (m_playState == "edit") {
            return;
        }
        world->setState(WorldState::Simulation);
        m_playState = "edit";
        break;
    }
    m_eventCallback(BackendEventMessage{"play_state_changed", m_playState});
}

} // namespace cakery
