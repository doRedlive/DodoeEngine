// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "adapters/runtime/services/FieldAttributes.h"
#include "adapters/runtime/services/TilePaintService.h"
#include "commands/ReparentEntityCommand.h"
#include "core/document/EditorDocumentSerializer.h"

#include "runtime/core/application.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/project/project.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_view/camera_provider.h"
#include "runtime/function/render/render_view/render_view_manager.h"
#include "runtime/function/render/render_view/render_view_target.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/tag_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/importer/import_settings_io.h"
#include "runtime/service/editor/picking_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

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

constexpr Float kHandleLength = 1.0f;
constexpr Float kArrowHeadLength = 0.15f;
constexpr Float kArrowHeadRadius = 0.04f;
constexpr UInt32 kArrowHeadSegments = 4;
constexpr Float kRingRadius = 0.9f;
constexpr UInt32 kRingSegments = 48;
constexpr Float kCubeHalfSize = 0.06f;
constexpr Float kGizmoHitThresholdPx = 12.0f;

const dodoe::Color kAxisRed{1.0f, 0.2f, 0.2f, 1.0f};
const dodoe::Color kAxisGreen{0.2f, 1.0f, 0.2f, 1.0f};
const dodoe::Color kAxisBlue{0.2f, 0.4f, 1.0f, 1.0f};
const dodoe::Vector3f kAxes[3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
};
const dodoe::Color kColors[3] = {kAxisRed, kAxisGreen, kAxisBlue};

[[nodiscard]] dodoe::GizmoVertex MakeVertex(const dodoe::Vector3f& pos, const dodoe::Color& color) {
    return {pos.x, pos.y, pos.z, color.r, color.g, color.b, color.a};
}

void AddLine(dodoe::GizmoChannelData& data, const dodoe::Vector3f& start, const dodoe::Vector3f& end,
             const dodoe::Color& color) {
    const UInt32 base = static_cast<UInt32>(data.vertices.size());
    data.vertices.push_back(MakeVertex(start, color));
    data.vertices.push_back(MakeVertex(end, color));

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base;
    cmd.vertex_count  = 2;
    cmd.index_offset  = 0;
    cmd.index_count   = 0;
    cmd.topology      = dodoe::GfxPrimitiveType::LineList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void AddArrowHead(dodoe::GizmoChannelData& data, const dodoe::Vector3f& tip, const dodoe::Vector3f& axis,
                  const dodoe::Color& color) {
    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index  = static_cast<UInt32>(data.indices.size());

    dodoe::Vector3f perp1, perp2;
    if (std::abs(axis.x) < 0.9f) {
        perp1 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, dodoe::Vector3f(1.0f, 0.0f, 0.0f)));
    } else {
        perp1 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, dodoe::Vector3f(0.0f, 1.0f, 0.0f)));
    }
    perp2 = dodoe::Math::Normalize(dodoe::Math::Cross(axis, perp1));

    const dodoe::Vector3f base_center = tip - axis * kArrowHeadLength;

    data.vertices.push_back(MakeVertex(tip, color));
    for (UInt32 i = 0; i < kArrowHeadSegments; ++i) {
        const Float angle = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kArrowHeadSegments);
        const dodoe::Vector3f offset = (perp1 * std::cos(angle) + perp2 * std::sin(angle)) * kArrowHeadRadius;
        data.vertices.push_back(MakeVertex(base_center + offset, color));
    }
    data.vertices.push_back(MakeVertex(base_center, color));

    const UInt32 tip_idx   = base_vertex;
    const UInt32 base_mid  = base_vertex + 1 + kArrowHeadSegments;

    for (UInt32 i = 0; i < kArrowHeadSegments; ++i) {
        const UInt32 curr = base_vertex + 1 + i;
        const UInt32 next = base_vertex + 1 + (i + 1) % kArrowHeadSegments;
        data.indices.push_back(tip_idx);
        data.indices.push_back(next);
        data.indices.push_back(curr);
        data.indices.push_back(base_mid);
        data.indices.push_back(curr);
        data.indices.push_back(next);
    }

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base_vertex;
    cmd.vertex_count  = static_cast<UInt32>(data.vertices.size()) - base_vertex;
    cmd.index_offset  = base_index;
    cmd.index_count   = static_cast<UInt32>(data.indices.size()) - base_index;
    cmd.topology      = dodoe::GfxPrimitiveType::TriangleList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void AddArrow(dodoe::GizmoChannelData& data, const dodoe::Vector3f& origin, const dodoe::Vector3f& axis,
              const dodoe::Color& color) {
    const dodoe::Vector3f tip = origin + axis * kHandleLength;
    const dodoe::Vector3f shaft_end = tip - axis * kArrowHeadLength;
    AddLine(data, origin, shaft_end, color);
    AddArrowHead(data, tip, axis, color);
}

void GenerateTranslateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    const dodoe::Matrix4f translation = dodoe::Math::Translate(dodoe::Matrix4f(1.0f), position);

    for (Int32 i = 0; i < 3; ++i) {
        dodoe::GizmoChannelData axis_data;
        AddArrow(axis_data, dodoe::Vector3f(0.0f), kAxes[i], kColors[i]);

        const UInt32 vertex_base = static_cast<UInt32>(data.vertices.size());
        const UInt32 index_base  = static_cast<UInt32>(data.indices.size());

        for (auto& v : axis_data.vertices) {
            dodoe::Vector4f world_pos = translation * dodoe::Vector4f(v.px, v.py, v.pz, 1.0f);
            v.px = world_pos.x; v.py = world_pos.y; v.pz = world_pos.z;
        }

        for (auto& cmd : axis_data.commands) {
            cmd.vertex_offset += vertex_base;
            cmd.index_offset  += index_base;
        }

        data.vertices.insert(data.vertices.end(), axis_data.vertices.begin(), axis_data.vertices.end());
        data.indices.insert(data.indices.end(), axis_data.indices.begin(), axis_data.indices.end());
        data.commands.insert(data.commands.end(), axis_data.commands.begin(), axis_data.commands.end());
    }

    data.has_data = true;
}

float PointDistanceSq(float px, float py, const dodoe::Vector2f& a) {
    const float dx = px - a.x;
    const float dy = py - a.y;
    return dx * dx + dy * dy;
}

float PointSegmentDistanceSq(float px, float py, const dodoe::Vector2f& a, const dodoe::Vector2f& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float lenSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (lenSq > 1e-8f) {
        t = ((px - a.x) * abx + (py - a.y) * aby) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = a.x + abx * t;
    const float cy = a.y + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy;
}

bool RayPlaneIntersect(const dodoe::Vector3f& origin, const dodoe::Vector3f& dir,
                       const dodoe::Vector3f& planePoint, const dodoe::Vector3f& planeNormal,
                       dodoe::Vector3f& outPoint) {
    const float denom = dodoe::Math::Dot(planeNormal, dir);
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    const float t = dodoe::Math::Dot(planeNormal, planePoint - origin) / denom;
    if (t < 0.0f) {
        return false;
    }
    outPoint = origin + dir * t;
    return true;
}

void AddCube(dodoe::GizmoChannelData& data, const dodoe::Vector3f& center, float halfSize,
             const dodoe::Color& color) {
    const UInt32 base_vertex = static_cast<UInt32>(data.vertices.size());
    const UInt32 base_index = static_cast<UInt32>(data.indices.size());

    const float h = halfSize;
    const dodoe::Vector3f corners[8] = {
        center + dodoe::Vector3f(-h, -h, -h),
        center + dodoe::Vector3f( h, -h, -h),
        center + dodoe::Vector3f( h,  h, -h),
        center + dodoe::Vector3f(-h,  h, -h),
        center + dodoe::Vector3f(-h, -h,  h),
        center + dodoe::Vector3f( h, -h,  h),
        center + dodoe::Vector3f( h,  h,  h),
        center + dodoe::Vector3f(-h,  h,  h),
    };
    const UInt32 faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0},
    };

    UInt32 face_base = base_vertex;
    for (const auto& face : faces) {
        for (UInt32 i = 0; i < 4; ++i) {
            data.vertices.push_back(MakeVertex(corners[face[i]], color));
        }
        data.indices.push_back(face_base + 0);
        data.indices.push_back(face_base + 1);
        data.indices.push_back(face_base + 2);
        data.indices.push_back(face_base + 0);
        data.indices.push_back(face_base + 2);
        data.indices.push_back(face_base + 3);
        face_base += 4;
    }

    dodoe::GizmoDrawCommand cmd;
    cmd.vertex_offset = base_vertex;
    cmd.vertex_count  = static_cast<UInt32>(data.vertices.size()) - base_vertex;
    cmd.index_offset  = base_index;
    cmd.index_count   = static_cast<UInt32>(data.indices.size()) - base_index;
    cmd.topology      = dodoe::GfxPrimitiveType::TriangleList;
    cmd.transform     = dodoe::Matrix4f(1.0f);
    data.commands.push_back(cmd);
}

void GenerateRotateGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    for (Int32 axis = 0; axis < 3; ++axis) {
        for (UInt32 i = 0; i < kRingSegments; ++i) {
            const Float a0 = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
            const Float a1 = static_cast<Float>(i + 1) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
            const Float c0 = std::cos(a0), s0 = std::sin(a0);
            const Float c1 = std::cos(a1), s1 = std::sin(a1);
            dodoe::Vector3f p0, p1;
            if (axis == 0) {
                p0 = position + dodoe::Vector3f(0.0f, c0, s0) * kRingRadius;
                p1 = position + dodoe::Vector3f(0.0f, c1, s1) * kRingRadius;
            } else if (axis == 1) {
                p0 = position + dodoe::Vector3f(c0, 0.0f, s0) * kRingRadius;
                p1 = position + dodoe::Vector3f(c1, 0.0f, s1) * kRingRadius;
            } else {
                p0 = position + dodoe::Vector3f(c0, s0, 0.0f) * kRingRadius;
                p1 = position + dodoe::Vector3f(c1, s1, 0.0f) * kRingRadius;
            }
            AddLine(data, p0, p1, kColors[axis]);
        }
    }
    data.has_data = true;
}

void GenerateScaleGizmo(dodoe::GizmoChannelData& data, const dodoe::Vector3f& position) {
    for (Int32 i = 0; i < 3; ++i) {
        const dodoe::Vector3f tip = position + kAxes[i] * kHandleLength;
        AddLine(data, position, tip, kColors[i]);
        AddCube(data, tip, kCubeHalfSize, kColors[i]);
    }
    data.has_data = true;
}

void SyncNativeComponents(dodoe::Entity entity, const std::vector<EditorComponent>& components) {
    auto& component_db = dodoe::ComponentDB::self();
    for (const auto& component : components) {
        if (component.typeName == "HierarchyComponent") continue;
        const auto* entry = component_db.find(
            dodoe::String(component.typeName.data(), component.typeName.size()));
        if (!entry || !entry->readJson) continue;
        if (!entry->contains(entity)) {
            if (!entry->canAdd()) continue;
            entry->add(entity);
        }
        void* ptr = entry->get(entity);
        if (!ptr) continue;
        (void)entry->readJson(ptr, component.value);
        if (entry->markDirty) {
            entry->markDirty(entity);
        }
    }
}

void SyncManagedComponents(dodoe::SystemContext& context, dodoe::Entity entity,
                           const std::vector<EditorComponent>& components)
{
    auto* scriptSystem = context.getScriptSystem();
    auto* scriptRuntime = scriptSystem ? scriptSystem->getScriptRuntime() : nullptr;
    if (!scriptRuntime) {
        return;
    }

    const std::uint64_t uuid = static_cast<std::uint64_t>(entity.uuid());
    dodoe::DynamicArray<dodoe::Pair<dodoe::String, dodoe::Json>> existing;
    scriptRuntime->getEntityManagedComponentFields(uuid, existing);

    std::unordered_set<std::string> desired;
    for (const auto& component : components) {
        desired.insert(component.typeName);
        bool present = false;
        for (const auto& [typeName, fields] : existing) {
            if (std::string(typeName.c_str()) == component.typeName) {
                present = true;
                break;
            }
        }
        if (!present) {
            scriptRuntime->addEntityManagedComponentFromManaged(
                uuid, dodoe::String(component.typeName.data(), component.typeName.size()));
        }
        scriptRuntime->setEntityManagedComponentFields(
            uuid,
            dodoe::String(component.typeName.data(), component.typeName.size()),
            component.value);
    }

    for (const auto& [typeName, fields] : existing) {
        if (!desired.contains(std::string(typeName.c_str()))) {
            scriptRuntime->removeEntityManagedComponentFromManaged(uuid, typeName);
        }
    }
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
        return true;
    }

    if (command.name == "gizmo_mode") {
        m_gizmoMode = command.payload.empty() ? "none" : command.payload;
        return true;
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
    if (!m_booted && !bootRuntime()) {
        return false;
    }
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
    if (m_hasPendingMetrics) {
        applyPendingMetrics();
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
    if (!m_booted) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    if (ctx) {
        ctx->getLayerStack().detach();
        ctx->stopRuntime();
        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());
        ctx->finalizeModules();
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

bool RuntimeEditorBackend::bootRuntime()
{
    if (m_booted) {
        return true;
    }

    const float bootW = m_pending.logicalWidth > 0 ? static_cast<float>(m_pending.logicalWidth) : 1280.0f;
    const float bootH = m_pending.logicalHeight > 0 ? static_cast<float>(m_pending.logicalHeight) : 720.0f;
    const int bootPixelW = m_pending.pixelWidth > 0 ? m_pending.pixelWidth : static_cast<int>(bootW);
    const int bootPixelH = m_pending.pixelHeight > 0 ? m_pending.pixelHeight : static_cast<int>(bootH);

    ApplicationSpecification spec;
    spec.name = "Cakery";
    spec.app_mode = AppMode::Editor;
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
    SystemContext& ctx = m_app->context();

    EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

    ctx.initializeModules();
    ctx.startRuntime();
    ctx.getLayerStack().attach();

    m_assetDatabase = std::make_unique<AssetDatabase>();
    m_assetDatabase->refresh();

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

    if (m_camera) {
        m_camera->setViewportSize(bootW, bootH);
    }

    // Window::getPixelSize() is initialized from the host metrics, so the
    // first render-system frame creates the swapchain at the same pixel size
    // as the Qt surface instead of the logical fallback size.
    if (auto* window = ctx.getWindowManager()->getWindow()) {
        window->setPixelSize(bootPixelW, bootPixelH);
    }

    m_lastTick = std::chrono::steady_clock::now();
    m_booted = true;
    m_state = BackendState::Ready;
    m_diagnostic = "Runtime backend booted.";
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

bool RuntimeEditorBackend::reconcileScene(const EditorDocument& document)
{
    if (m_playState != "edit") {
        return true;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return false;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        scene = world->createScene(document.name.empty()
            ? dodoe::String("Untitled")
            : dodoe::String(document.name.data(), document.name.size()));
        if (!scene) {
            return false;
        }
        world->setActiveScene(scene);
    } else if (!document.name.empty() && scene->getName().c_str() != document.name) {
        scene->setName(dodoe::String(document.name.data(), document.name.size()));
    }

    for (const EditorEntity& entity : document.entities) {
        const dodoe::UUID uuid(entity.uuid);
        Entity sceneEntity = scene->tryGetEntityByUUID(uuid);
        if (!sceneEntity.valid()) {
            sceneEntity = scene->createEntity(
                uuid, dodoe::String(entity.name.data(), entity.name.size()));
        } else if (sceneEntity.name().c_str() != entity.name) {
            sceneEntity.getComponent<IDComponent>().setName(
                dodoe::String(entity.name.data(), entity.name.size()));
        }
        SyncNativeComponents(sceneEntity, entity.nativeComponents);
        SyncManagedComponents(*ctx, sceneEntity, entity.managedComponents);
    }

    for (Entity sceneEntity : scene->getEntities()) {
        if (sceneEntity.hasComponent<TagComponent>() &&
            sceneEntity.getComponent<TagComponent>().tag == "PrimaryCamera") {
            continue;
        }
        bool keep = false;
        for (const EditorEntity& entity : document.entities) {
            if (entity.uuid == static_cast<std::uint64_t>(sceneEntity.uuid())) {
                keep = true;
                break;
            }
        }
        if (!keep) {
            scene->destroyEntity(sceneEntity);
        }
    }

    return true;
}

void RuntimeEditorBackend::updateGizmo()
{
    dodoe::GizmoChannelData& channel_data = dodoe::GetGizmoChannel().get<dodoe::GizmoChannelData>();
    channel_data.clear();
    if (m_gizmoMode == "none" || m_selectedUuid == 0) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }
    dodoe::Entity entity = scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>()) {
        return;
    }
    const dodoe::Vector3f position = entity.getComponent<dodoe::TransformComponent>().getPosition();
    if (m_gizmoMode == "translate") {
        GenerateTranslateGizmo(channel_data, position);
    } else if (m_gizmoMode == "rotate") {
        GenerateRotateGizmo(channel_data, position);
    } else if (m_gizmoMode == "scale") {
        GenerateScaleGizmo(channel_data, position);
    }
}

void RuntimeEditorBackend::pickAt(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }
    dodoe::Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    dodoe::Entity entity = dodoe::PickingBackend::RaycastNearest(*scene, origin, dir);
    if (!entity.valid()) {
        // A miss in the current lightweight picker is not proof that the user
        // intended to clear the selection. Keep the Inspector stable until a
        // real entity hit is reported.
        return;
    }
    m_selectedUuid = static_cast<std::uint64_t>(entity.uuid());
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"selection_changed", std::to_string(m_selectedUuid)});
    }
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

dodoe::Entity RuntimeEditorBackend::selectedSceneEntity() const
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return {};
    }
    Scene* scene = world->getActiveScene();
    if (!scene || m_selectedUuid == 0) {
        return {};
    }
    return scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
}

int RuntimeEditorBackend::hitTestGizmo(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return -1;
    }
    const dodoe::Vector3f center = entity.getComponent<dodoe::TransformComponent>().getPosition();
    const float thresholdSq = kGizmoHitThresholdPx * kGizmoHitThresholdPx;
    int bestAxis = -1;
    float bestDist = thresholdSq;

    if (m_gizmoMode == "rotate") {
        for (Int32 axis = 0; axis < 3; ++axis) {
            float minDist = 1e30f;
            for (UInt32 i = 0; i < kRingSegments; ++i) {
                const Float a = static_cast<Float>(i) * 2.0f * 3.14159265f / static_cast<Float>(kRingSegments);
                const Float c = std::cos(a), s = std::sin(a);
                dodoe::Vector3f point;
                if (axis == 0) {
                    point = center + dodoe::Vector3f(0.0f, c, s) * kRingRadius;
                } else if (axis == 1) {
                    point = center + dodoe::Vector3f(c, 0.0f, s) * kRingRadius;
                } else {
                    point = center + dodoe::Vector3f(c, s, 0.0f) * kRingRadius;
                }
                const dodoe::Vector2f screenPt = m_camera->projectToScreen(point);
                minDist = std::min(minDist, PointDistanceSq(screenX, screenY, screenPt));
            }
            if (minDist < bestDist) {
                bestDist = minDist;
                bestAxis = axis;
            }
        }
        return bestAxis;
    }

    for (Int32 axis = 0; axis < 3; ++axis) {
        const dodoe::Vector2f start = m_camera->projectToScreen(center);
        const dodoe::Vector2f end = m_camera->projectToScreen(center + kAxes[axis] * kHandleLength);
        const float dist = PointSegmentDistanceSq(screenX, screenY, start, end);
        if (dist < bestDist) {
            bestDist = dist;
            bestAxis = axis;
        }
    }
    return bestAxis;
}

void RuntimeEditorBackend::beginDrag(int axis, float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>()) {
        return;
    }
    auto& transform = entity.getComponent<dodoe::TransformComponent>();
    m_dragMode = m_gizmoMode;
    m_dragAxis = axis;
    m_dragStartPosition = transform.getPosition();
    m_dragStartRotation = transform.getRotation();
    m_dragStartScale = transform.getScale();

    if (!m_camera) {
        m_dragAxis = -1;
        m_dragMode.clear();
        return;
    }

    if (m_dragMode == "translate" || m_dragMode == "scale") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), m_dragPlanePoint)) {
            m_dragAxis = -1;
            m_dragMode.clear();
            return;
        }
    }

    if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        // A view-facing axis has no screen-space direction in a 2D/editor
        // view. It cannot provide a meaningful drag distance.
        if (axisLengthSq < 1e-6f) {
            m_dragAxis = -1;
            m_dragMode.clear();
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        m_dragStartAxisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
    }

    if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        m_dragStartAngle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
    }

    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"transform_drag_begin", ""});
    }
}

void RuntimeEditorBackend::updateDrag(float screenX, float screenY)
{
    dodoe::Entity entity = selectedSceneEntity();
    if (!entity || !entity.hasComponent<dodoe::TransformComponent>() || !m_camera) {
        return;
    }
    auto& transform = entity.getComponent<dodoe::TransformComponent>();

    if (m_dragMode == "translate") {
        dodoe::Vector3f origin, dir;
        m_camera->screenToRay(screenX, screenY, origin, dir);
        dodoe::Vector3f planePoint;
        if (!RayPlaneIntersect(origin, dir, m_dragStartPosition, m_camera->forwardDirection(), planePoint)) {
            return;
        }
        const float movement = dodoe::Math::Dot(planePoint - m_dragPlanePoint, kAxes[m_dragAxis]);
        const dodoe::Vector3f newPosition = m_dragStartPosition + kAxes[m_dragAxis] * movement;
        transform.setPosition(newPosition);
        emitTransformChange(newPosition, transform.getRotation(), transform.getScale());
    } else if (m_dragMode == "rotate") {
        const dodoe::Vector2f centerScreen = m_camera->projectToScreen(m_dragStartPosition);
        const float angle = std::atan2(screenY - centerScreen.y, screenX - centerScreen.x);
        const float deltaDegrees = (angle - m_dragStartAngle) * 180.0f / 3.14159265f;
        dodoe::Vector3f newRotation = m_dragStartRotation;
        newRotation[m_dragAxis] += deltaDegrees;
        transform.setRotation(newRotation);
        emitTransformChange(transform.getPosition(), newRotation, transform.getScale());
    } else if (m_dragMode == "scale") {
        const dodoe::Vector2f start = m_camera->projectToScreen(m_dragStartPosition);
        const dodoe::Vector2f end = m_camera->projectToScreen(
            m_dragStartPosition + kAxes[m_dragAxis] * kHandleLength);
        const dodoe::Vector2f axisScreen = end - start;
        const float axisLengthSq = axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y;
        if (axisLengthSq < 1e-6f) {
            return;
        }
        const dodoe::Vector2f mouseOffset{screenX - start.x, screenY - start.y};
        const float axisParam = (mouseOffset.x * axisScreen.x + mouseOffset.y * axisScreen.y) / axisLengthSq;
        const float movement = axisParam - m_dragStartAxisParam;
        dodoe::Vector3f newScale = m_dragStartScale;
        newScale[m_dragAxis] = std::max(0.01f,
            m_dragStartScale[m_dragAxis] + movement * kHandleLength);
        transform.setScale(newScale);
        emitTransformChange(transform.getPosition(), transform.getRotation(), newScale);
    }

    if (m_camera) {
        m_camera->updateLastMouse(screenX, screenY);
    }
}

void RuntimeEditorBackend::endDrag()
{
    m_dragAxis = -1;
    m_dragMode.clear();
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"transform_drag_end", ""});
    }
}

void RuntimeEditorBackend::emitTransformChange(const dodoe::Vector3f& position,
                                               const dodoe::Vector3f& rotation,
                                               const dodoe::Vector3f& scale)
{
    if (!m_eventCallback || m_playState != "edit") {
        return;
    }
    nlohmann::json payload = {
        {"uuid", m_selectedUuid},
        {"value", {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale", {scale.x, scale.y, scale.z}},
        }},
    };
    m_eventCallback(BackendEventMessage{"transform_changed", payload.dump()});
}

} // namespace cakery
