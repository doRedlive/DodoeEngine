// do@Redlive

#include "dopch.h"

#include "TilemapEditController.h"

#include "backend/EditorCamera.h"
#include "core/EditorSession.h"
#include "core/bridge/EditorTypes.h"
#include "services/TileCoord.h"
#include "services/UuidResolve.h"
#include "core/commands/CreateTileLayerCommand.h"
#include "core/commands/CreateTilemapCommand.h"
#include "core/commands/CreateTilesetCommand.h"
#include "core/commands/RemoveTilesetCommand.h"
#include "core/commands/ReorderTileLayerCommand.h"
#include "core/commands/ResizeTilemapCommand.h"
#include "core/commands/UpdateTilesetCommand.h"
#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/history/EditorHistory.h"
#include "services/TilePaintService.h"

#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace dodoe;

namespace cakery {

    using dodoe::UUID;

namespace {

const char* TileToolName(TileTool tool)
{
    switch (tool) {
    case TileTool::Select: return "select";
    case TileTool::Brush:  return "brush";
    case TileTool::Erase:  return "erase";
    case TileTool::Fill:   return "fill";
    case TileTool::Rect:   return "rect";
    case TileTool::Line:   return "line";
    case TileTool::Picker: return "picker";
    }
    return "select";
}

TileTool MapTileTool(TileToolId tool)
{
    switch (tool) {
    case TileToolId::Brush:  return TileTool::Brush;
    case TileToolId::Erase:   return TileTool::Erase;
    case TileToolId::Fill:    return TileTool::Fill;
    case TileToolId::Rect:    return TileTool::Rect;
    case TileToolId::Line:    return TileTool::Line;
    case TileToolId::Picker:  return TileTool::Picker;
    case TileToolId::Select:  break;
    }
    return TileTool::Select;
}

} // anonymous namespace

TilemapEditController::TilemapEditController(EditorSession& session)
    : m_session(session)
{
    TilePaintService::RegisterCommands(session.commands());
    m_paint = create_scope<TilePaintService>(session);
}

TilemapEditController::~TilemapEditController() = default;

void TilemapEditController::setSceneProvider(std::function<dodoe::Scene*()> provider)
{
    m_sceneProvider = std::move(provider);
}

void TilemapEditController::setCamera(EditorCamera* camera)
{
    m_camera = camera;
}

void TilemapEditController::setPublisher(
    std::function<void(const BackendEventMessage&)> publisher)
{
    m_publisher = std::move(publisher);
}

dodoe::Scene* TilemapEditController::activeScene() const
{
    return m_sceneProvider ? m_sceneProvider() : nullptr;
}

void TilemapEditController::publish(const BackendEventMessage& message) const
{
    if (m_publisher) {
        m_publisher(message);
    }
}

bool TilemapEditController::handleCommand(const TilemapCreateCommand& command)
{
    auto inner = create_scope<CreateTilemapCommand>(
        command.name, command.width, command.height,
        command.tileWidth, command.tileHeight);
    auto* executed = m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    if (executed) {
        const UUID tilemapUuid =
            static_cast<CreateTilemapCommand*>(executed)->created();
        if (tilemapUuid.isValid()) {
            m_session.selection().set(static_cast<UInt64>(tilemapUuid));
            updateFromSelection();
        }
    }
    return true;
}

bool TilemapEditController::handleCommand(const TilemapActivateCommand& command)
{
    if (command.uuid == 0) return false;
    m_paint->setActiveEntity(UUID(command.uuid));
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (tilemapUuid.isValid()) {
        activateEdit(tilemapUuid, m_paint->activeLayer());
    }
    return true;
}

bool TilemapEditController::handleCommand(const TilemapToolCommand& command)
{
    m_paint->setTool(MapTileTool(command.tool));
    return true;
}

bool TilemapEditController::handleCommand(const TilemapBrushFlipCommand& command)
{
    if (command.horizontal) {
        m_paint->flipBrushX();
    } else {
        m_paint->flipBrushY();
    }
    return true;
}

bool TilemapEditController::handleCommand(const TilemapBrushRotateCommand&)
{
    m_paint->rotateBrushCW();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapRandomBrushCommand& command)
{
    m_paint->setRandomBrush(command.enabled);
    return true;
}

bool TilemapEditController::handleCommand(const TilemapSelectionDeleteCommand&)
{
    m_paint->deleteSelection();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapSelectionCopyCommand&)
{
    m_paint->copySelection();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapSelectionPasteCommand&)
{
    m_paint->pasteClipboard();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapEditTilesetCommand& command)
{
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.assetId == 0 || command.tileWidth == 0 || command.tileHeight == 0) return false;
    auto inner = create_scope<UpdateTilesetCommand>(
        tilemapUuid, UUID(command.assetId), command.tileWidth, command.tileHeight,
        command.margin, command.spacing);
    m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapBrushCommand& command)
{
    TileBrush brush;
    brush.w = std::max(1, command.width);
    brush.h = std::max(1, command.height);
    brush.gids = command.gids;
    const Size_t expected =
        static_cast<Size_t>(brush.w) * static_cast<Size_t>(brush.h);
    if (brush.gids.size() < expected) {
        brush.gids.resize(expected, 0);
    }
    m_paint->setBrush(brush);
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerActiveCommand& command)
{
    if (command.uuid == 0) return false;
    m_paint->setActiveLayer(UUID(command.uuid));
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerVisibleCommand& command)
{
    if (command.uuid == 0) return false;
    executeLayerField(UUID(command.uuid), "visible", command.visible);
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerOpacityCommand& command)
{
    if (command.uuid == 0) return false;
    executeLayerField(UUID(command.uuid), "opacity", command.opacity);
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerRenameCommand& command)
{
    if (command.uuid == 0 || command.name.empty()) return false;
    return m_session.renameEntity(
        command.uuid, String(command.name.c_str(), command.name.size()));
}

bool TilemapEditController::handleCommand(const TilemapLayerAddCommand& command)
{
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    const String name = command.name.empty() ? String("Layer") : command.name;
    dodoe::Scene* scene = activeScene();
    Entity tm = scene ? ResolveEntity(scene, tilemapUuid) : Entity();
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;
    const auto& comp = tm.getComponent<TilemapComponent>();
    auto inner = create_scope<CreateTileLayerCommand>(
        tilemapUuid, name, comp.map_width, comp.map_height);
    auto* executed = m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    if (executed) {
        const UUID layerUuid = static_cast<CreateTileLayerCommand*>(executed)->created();
        if (layerUuid.isValid()) {
            m_paint->setActiveLayer(layerUuid);
        }
    }
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerMoveCommand& command)
{
    if (command.uuid == 0) return false;
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    auto inner = create_scope<ReorderTileLayerCommand>(
        tilemapUuid, UUID(command.uuid), command.up);
    m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapLayerRemoveCommand& command)
{
    if (command.uuid == 0) return false;
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (tilemapUuid.isValid()) {
        EditorEntity* tilemapEntity =
            m_session.documentModel().findEntity(static_cast<UInt64>(tilemapUuid));
        if (tilemapEntity) {
            for (auto& component : tilemapEntity->nativeComponents) {
                if (component.typeName != "TilemapComponent" ||
                    !component.value.contains("layer_order")) {
                    continue;
                }
                auto& order = component.value["layer_order"];
                order.erase(std::remove(order.begin(), order.end(), command.uuid), order.end());
                break;
            }
        }
    }
    m_session.deleteEntity(command.uuid);
    return true;
}

bool TilemapEditController::handleCommand(const TilemapAddTilesetCommand& command)
{
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.image.empty()) return false;
    auto inner = create_scope<CreateTilesetCommand>(
        tilemapUuid, command.image, command.tileWidth, command.tileHeight,
        command.margin, command.spacing);
    m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapResizeCommand& command)
{
    if (command.uuid == 0 || command.width == 0 || command.height == 0) return false;
    auto inner = create_scope<ResizeTilemapCommand>(
        UUID(command.uuid), command.width, command.height);
    m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    return true;
}

bool TilemapEditController::handleCommand(const TilemapRemoveTilesetCommand& command)
{
    const UUID tilemapUuid = m_paint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.assetId == 0) return false;
    auto inner = create_scope<RemoveTilesetCommand>(tilemapUuid, UUID(command.assetId));
    m_session.history().execute(std::move(inner), m_session.documentModel());
    m_session.notifyDocumentChanged();
    return true;
}

bool TilemapEditController::queryState(const String& tilemapUuid, Json& out) const
{
    out = nullptr;

    UUID activeUuid = m_paint->activeTilemap();
    if (!tilemapUuid.empty()) {
        const UInt64 parsed = std::strtoull(tilemapUuid.c_str(), nullptr, 10);
        if (parsed != 0) activeUuid = UUID(parsed);
    }
    if (!activeUuid.isValid()) return false;

    dodoe::Scene* scene = activeScene();
    if (!scene) return false;
    Entity tm = ResolveEntity(scene, activeUuid);
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;

    const auto& comp = tm.getComponent<TilemapComponent>();

    Json tilemap;
    tilemap["uuid"] = static_cast<UInt64>(activeUuid);
    tilemap["name"] = tm.name();
    tilemap["map_width"] = comp.map_width;
    tilemap["map_height"] = comp.map_height;
    tilemap["tile_width"] = comp.tile_width;
    tilemap["tile_height"] = comp.tile_height;

    Json layers = Json::array();
    if (tm.hasComponent<HierarchyComponent>()) {
        for (auto& child : tm.getComponent<HierarchyComponent>().children) {
            if (!child.valid() || !child.hasComponent<TileLayerComponent>()) continue;
            const auto& layer = child.getComponent<TileLayerComponent>();
            Json item;
            item["uuid"] = static_cast<UInt64>(child.uuid());
            item["name"] = child.name();
            item["visible"] = layer.visible;
            item["opacity"] = layer.opacity;
            item["active"] = child.uuid() == m_paint->activeLayer();
            layers.push_back(std::move(item));
        }
    }

    Json tilesets = Json::array();
    for (const auto& ref : comp.tilesets) {
        const Tileset* tileset = ref.get();
        if (!tileset) continue;
        Json item;
        item["asset_id"] = static_cast<UInt64>(ref.getObjectID().asset_id);
        item["name"] = tileset->name;
        item["image_path"] = tileset->image_path;
        item["tile_width"] = tileset->tile_width;
        item["tile_height"] = tileset->tile_height;
        item["columns"] = tileset->columns;
        item["tile_count"] = tileset->tile_count;
        item["first_gid"] = tileset->first_gid;
        item["margin"] = tileset->margin;
        item["spacing"] = tileset->spacing;
        tilesets.push_back(std::move(item));
    }

    Json brush;
    brush["w"] = m_paint->brush().w;
    brush["h"] = m_paint->brush().h;
    brush["gids"] = Json::array();
    for (UInt32 gid : m_paint->brush().gids) {
        brush["gids"].push_back(gid);
    }

    out["tilemap"] = std::move(tilemap);
    out["active_layer"] = static_cast<UInt64>(m_paint->activeLayer());
    out["layers"] = std::move(layers);
    out["tilesets"] = std::move(tilesets);
    out["tool"] = TileToolName(m_paint->tool());
    out["brush"] = std::move(brush);
    out["random_brush"] = m_paint->randomBrush();
    return true;
}

void TilemapEditController::emitEditMode(bool active)
{
    publish(BackendEventMessage{"tilemap_edit_mode", active ? "1" : "0"});
}

void TilemapEditController::updateFromSelection()
{
    const UInt64 selected = m_session.selection().target() == EditorSelection::Target::Entity
        ? m_session.selection().selected() : 0;
    if (selected == 0) {
        emitEditMode(false);
        return;
    }
    dodoe::Scene* scene = activeScene();
    if (!scene) {
        emitEditMode(false);
        return;
    }
    Entity entity = scene->tryGetEntityByUUID(UUID(selected));
    if (!entity.valid()) {
        emitEditMode(false);
        return;
    }
    if (entity.hasComponent<TilemapComponent>()) {
        const UUID tilemapUuid = entity.uuid();
        m_paint->setActiveEntity(tilemapUuid);
        activateEdit(tilemapUuid, m_paint->activeLayer());
    } else if (entity.hasComponent<TileLayerComponent>()) {
        const UUID layerUuid = entity.uuid();
        m_paint->setActiveEntity(layerUuid);
        const UUID tilemapUuid = m_paint->activeTilemap();
        if (tilemapUuid.isValid()) {
            activateEdit(tilemapUuid, layerUuid);
        } else {
            emitEditMode(false);
        }
    } else {
        emitEditMode(false);
    }
}

void TilemapEditController::activateEdit(const UUID& tilemapUuid, const UUID& layerUuid)
{
    if (layerUuid.isValid()) {
        m_paint->setActiveLayer(layerUuid);
    }
    if (m_camera) {
        dodoe::Scene* scene = activeScene();
        Entity tm = scene ? ResolveEntity(scene, tilemapUuid) : Entity();
        if (tm.valid() && tm.hasComponent<TilemapComponent>()) {
            const auto& comp = tm.getComponent<TilemapComponent>();
            const Vector3f center{
                static_cast<float>(comp.map_width * comp.tile_width) * 0.5f,
                static_cast<float>(comp.map_height * comp.tile_height) * 0.5f,
                0.0f};
            const float radius = std::max(static_cast<float>(comp.map_width * comp.tile_width),
                                          static_cast<float>(comp.map_height * comp.tile_height)) * 0.5f;
            m_camera->focusOn(center, radius);
            m_camera->setMode(EditorCamera::Mode::Ortho2D);
        }
    }
    publish(BackendEventMessage{"camera_mode_changed", "2d"});
    emitEditMode(true);
}

bool TilemapEditController::screenToCell(float screenX, float screenY, int& outX, int& outY) const
{
    if (!m_camera || !m_paint->hasTarget()) return false;
    dodoe::Scene* scene = activeScene();
    if (!scene) return false;
    Entity tm = ResolveEntity(scene, m_paint->activeTilemap());
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;

    Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    if (std::abs(dir.z) < 1e-6f) return false;
    const float t = -origin.z / dir.z;
    if (t < 0.0f) return false;
    const Vector3f worldPos = origin + dir * t;
    return TileCoord::worldToCell(tm.getComponent<TilemapComponent>(),
                                  Matrix4f(1.0f), worldPos, outX, outY);
}

void TilemapEditController::executeLayerField(dodoe::UUID layer, const String& field,
                                              const Json& value)
{
    EditorEntity* entity = m_session.documentModel().findEntity(static_cast<UInt64>(layer));
    if (!entity) return;
    for (Size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName != "TileLayerComponent") continue;
        auto command = create_scope<SetFieldValueCommand>(
            static_cast<UInt64>(layer), i, field, value);
        m_session.history().execute(std::move(command), m_session.documentModel());
        m_session.notifyDocumentChanged();
        return;
    }
}

void TilemapEditController::updateHover(float screenX, float screenY)
{
    if (!m_paint->hasTarget() || m_paint->tool() == TileTool::Select) return;
    if (!activeTilemapEntity().valid()) return;
    int cx = 0, cy = 0;
    if (screenToCell(screenX, screenY, cx, cy)) {
        m_paint->setHoverCell(cx, cy);
    } else {
        m_paint->clearHover();
    }
}

dodoe::Entity TilemapEditController::activeTilemapEntity() const
{
    if (!m_paint || !m_paint->hasTarget()) return {};
    dodoe::Scene* scene = activeScene();
    if (!scene) return {};
    return ResolveEntity(scene, m_paint->activeTilemap());
}

void TilemapEditController::invalidateTarget()
{
    m_paint->setActiveTilemap(UUID(0));
    m_paint->setActiveLayer(UUID(0));
    emitEditMode(false);
}

} // namespace cakery
