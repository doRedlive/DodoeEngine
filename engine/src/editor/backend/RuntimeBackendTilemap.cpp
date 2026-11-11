// do@Redlive

#include "dopch.h"

#include "RuntimeBackend.h"

#include "EditorCamera.h"
#include "core/EditorSession.h"
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

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
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

bool RuntimeBackend::handleCommand(const TilemapCreateCommand& command)
{
    auto command2 = create_scope<CreateTilemapCommand>(
        command.name, command.width, command.height,
        command.tileWidth, command.tileHeight);
    auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    if (executed) {
        const UUID tilemapUuid =
            static_cast<CreateTilemapCommand*>(executed)->created();
        if (tilemapUuid.isValid()) {
            m_session->selection().set(static_cast<UInt64>(tilemapUuid));
            updateTileEditFromSelection();
        }
    }
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapActivateCommand& command)
{
    if (command.uuid == 0) return false;
    m_tilePaint->setActiveEntity(UUID(command.uuid));
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (tilemapUuid.isValid()) {
        activateTilemapEdit(tilemapUuid, m_tilePaint->activeLayer());
    }
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapToolCommand& command)
{
    m_tilePaint->setTool(MapTileTool(command.tool));
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapBrushFlipCommand& command)
{
    if (command.horizontal) {
        m_tilePaint->flipBrushX();
    } else {
        m_tilePaint->flipBrushY();
    }
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapBrushRotateCommand&)
{
    m_tilePaint->rotateBrushCW();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapRandomBrushCommand& command)
{
    m_tilePaint->setRandomBrush(command.enabled);
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapSelectionDeleteCommand&)
{
    m_tilePaint->deleteSelection();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapSelectionCopyCommand&)
{
    m_tilePaint->copySelection();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapSelectionPasteCommand&)
{
    m_tilePaint->pasteClipboard();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapEditTilesetCommand& command)
{
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.assetId == 0 || command.tileWidth == 0 || command.tileHeight == 0) return false;
    auto command2 = create_scope<UpdateTilesetCommand>(
        tilemapUuid, UUID(command.assetId), command.tileWidth, command.tileHeight,
        command.margin, command.spacing);
    m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapBrushCommand& command)
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
    m_tilePaint->setBrush(brush);
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerActiveCommand& command)
{
    if (command.uuid == 0) return false;
    m_tilePaint->setActiveLayer(UUID(command.uuid));
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerVisibleCommand& command)
{
    if (command.uuid == 0) return false;
    executeTilemapLayerField(UUID(command.uuid), "visible", command.visible);
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerOpacityCommand& command)
{
    if (command.uuid == 0) return false;
    executeTilemapLayerField(UUID(command.uuid), "opacity", command.opacity);
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerRenameCommand& command)
{
    if (command.uuid == 0 || command.name.empty()) return false;
    return m_session->renameEntity(
        command.uuid, String(command.name.c_str(), command.name.size()));
}

bool RuntimeBackend::handleCommand(const TilemapLayerAddCommand& command)
{
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    const String name = command.name.empty() ? String("Layer") : command.name;
    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    Entity tm = scene ? ResolveEntity(scene, tilemapUuid) : Entity();
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;
    const auto& comp = tm.getComponent<TilemapComponent>();
    auto command2 = create_scope<CreateTileLayerCommand>(
        tilemapUuid, name, comp.map_width, comp.map_height);
    auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    if (executed) {
        const UUID layerUuid = static_cast<CreateTileLayerCommand*>(executed)->created();
        if (layerUuid.isValid()) {
            m_tilePaint->setActiveLayer(layerUuid);
        }
    }
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerMoveCommand& command)
{
    if (command.uuid == 0) return false;
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    auto command2 = create_scope<ReorderTileLayerCommand>(
        tilemapUuid, UUID(command.uuid), command.up);
    m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapLayerRemoveCommand& command)
{
    if (command.uuid == 0) return false;
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (tilemapUuid.isValid()) {
        EditorEntity* tilemapEntity =
            m_session->documentModel().findEntity(static_cast<UInt64>(tilemapUuid));
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
    m_session->deleteEntity(command.uuid);
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapAddTilesetCommand& command)
{
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.image.empty()) return false;
    auto command2 = create_scope<CreateTilesetCommand>(
        tilemapUuid, command.image, command.tileWidth, command.tileHeight,
        command.margin, command.spacing);
    m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapResizeCommand& command)
{
    if (command.uuid == 0 || command.width == 0 || command.height == 0) return false;
    auto command2 = create_scope<ResizeTilemapCommand>(
        UUID(command.uuid), command.width, command.height);
    m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    return true;
}

bool RuntimeBackend::handleCommand(const TilemapRemoveTilesetCommand& command)
{
    const UUID tilemapUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.isValid()) return false;
    if (command.assetId == 0) return false;
    auto command2 = create_scope<RemoveTilesetCommand>(tilemapUuid, UUID(command.assetId));
    m_session->history().execute(std::move(command2), m_session->documentModel());
    m_session->notifyDocumentChanged();
    return true;
}

bool RuntimeBackend::queryTilemapState(const String& tilemapUuid, Json& out) const
{
    out = nullptr;

    UUID activeUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.empty()) {
        const UInt64 parsed = std::strtoull(tilemapUuid.c_str(), nullptr, 10);
        if (parsed != 0) activeUuid = UUID(parsed);
    }
    if (!activeUuid.isValid()) return false;

    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
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
            item["active"] = child.uuid() == m_tilePaint->activeLayer();
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
    brush["w"] = m_tilePaint->brush().w;
    brush["h"] = m_tilePaint->brush().h;
    brush["gids"] = Json::array();
    for (UInt32 gid : m_tilePaint->brush().gids) {
        brush["gids"].push_back(gid);
    }

    out["tilemap"] = std::move(tilemap);
    out["active_layer"] = static_cast<UInt64>(m_tilePaint->activeLayer());
    out["layers"] = std::move(layers);
    out["tilesets"] = std::move(tilesets);
    out["tool"] = TileToolName(m_tilePaint->tool());
    out["brush"] = std::move(brush);
    out["random_brush"] = m_tilePaint->randomBrush();
    return true;
}

void RuntimeBackend::emitTilemapEditMode(bool active)
{
    m_eventCallback(BackendEventMessage{"tilemap_edit_mode", active ? "1" : "0"});
}

void RuntimeBackend::updateTileEditFromSelection()
{
    if (m_selectedUuid == 0) {
        emitTilemapEditMode(false);
        return;
    }
    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) {
        emitTilemapEditMode(false);
        return;
    }
    Entity entity = scene->tryGetEntityByUUID(UUID(m_selectedUuid));
    if (!entity.valid()) {
        emitTilemapEditMode(false);
        return;
    }
    if (entity.hasComponent<TilemapComponent>()) {
        const UUID tilemapUuid = entity.uuid();
        m_tilePaint->setActiveEntity(tilemapUuid);
        activateTilemapEdit(tilemapUuid, m_tilePaint->activeLayer());
    } else if (entity.hasComponent<TileLayerComponent>()) {
        const UUID layerUuid = entity.uuid();
        m_tilePaint->setActiveEntity(layerUuid);
        const UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (tilemapUuid.isValid()) {
            activateTilemapEdit(tilemapUuid, layerUuid);
        } else {
            emitTilemapEditMode(false);
        }
    } else {
        emitTilemapEditMode(false);
    }
}

void RuntimeBackend::activateTilemapEdit(const UUID& tilemapUuid, const UUID& layerUuid)
{
    if (layerUuid.isValid()) {
        m_tilePaint->setActiveLayer(layerUuid);
    }
    if (m_camera) {
        World* world = runtimeWorld();
        Scene* scene = world ? world->getActiveScene() : nullptr;
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
    m_eventCallback(BackendEventMessage{"camera_mode_changed", "2d"});
    emitTilemapEditMode(true);
}

bool RuntimeBackend::screenToCell(float screenX, float screenY, int& outX, int& outY) const
{
    if (!m_tilePaint->hasTarget()) return false;
    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return false;
    Entity tm = ResolveEntity(scene, m_tilePaint->activeTilemap());
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

void RuntimeBackend::executeTilemapLayerField(UUID layer, const String& field,
                                                     const Json& value)
{
    EditorEntity* entity = m_session->documentModel().findEntity(static_cast<UInt64>(layer));
    if (!entity) return;
    for (Size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName != "TileLayerComponent") continue;
        auto command = create_scope<SetFieldValueCommand>(
            static_cast<UInt64>(layer), i, field, value);
        m_session->history().execute(std::move(command), m_session->documentModel());
        m_session->notifyDocumentChanged();
        return;
    }
}

Entity RuntimeBackend::activeTilemapEntity() const
{
    if (!m_tilePaint || !m_tilePaint->hasTarget()) return {};
    World* world = runtimeWorld();
    Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return {};
    return ResolveEntity(scene, m_tilePaint->activeTilemap());
}

} // namespace cakery
