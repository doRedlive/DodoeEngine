// do@Redlive

#include "RuntimeEditorBackend.h"

#include "EditorCamera.h"
#include "core/EditorSession.h"
#include "adapters/runtime/services/TileCoord.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "commands/CreateTileLayerCommand.h"
#include "commands/CreateTilemapCommand.h"
#include "commands/CreateTilesetCommand.h"
#include "commands/RemoveTilesetCommand.h"
#include "commands/ReorderTileLayerCommand.h"
#include "commands/ResizeTilemapCommand.h"
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

} // anonymous namespace

bool RuntimeEditorBackend::executeTilemapCommand(const EditorCommandMessage& command)
{
    if (command.name == "tilemap.create") {
        if (!m_session) return false;
        try {
            const nlohmann::json payload = command.payload.empty()
                ? nlohmann::json::object()
                : nlohmann::json::parse(command.payload);
            std::string name = payload.value("name", std::string("Tilemap"));
            const dodoe::UInt32 w = payload.value("width", 20u);
            const dodoe::UInt32 h = payload.value("height", 20u);
            const dodoe::UInt32 tw = payload.value("tile_width", 16u);
            const dodoe::UInt32 th = payload.value("tile_height", 16u);
            auto command2 = std::make_unique<CreateTilemapCommand>(
                dodoe::String(name.c_str()), w, h, tw, th);
            auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
            m_session->notifyDocumentChanged();
            if (executed) {
                const dodoe::UUID tilemapUuid =
                    static_cast<CreateTilemapCommand*>(executed)->created();
                if (tilemapUuid.isValid()) {
                    m_session->selection().set(static_cast<std::uint64_t>(tilemapUuid));
                    updateTileEditFromSelection();
                }
            }
        } catch (const nlohmann::json::exception&) {
            return false;
        }
        return true;
    }

    if (command.name == "tilemap.activate") {
        if (!m_tilePaint) return false;
        const std::uint64_t uuid = command.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
        if (uuid == 0) return false;
        m_tilePaint->setActiveEntity(dodoe::UUID(uuid));
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (tilemapUuid.isValid()) {
            activateTilemapEdit(tilemapUuid, m_tilePaint->activeLayer());
        }
        return true;
    }

    if (command.name == "tilemap.tool") {
        if (!m_tilePaint) return false;
        TileTool tool = TileTool::Select;
        if (command.payload == "brush") tool = TileTool::Brush;
        else if (command.payload == "fill") tool = TileTool::Fill;
        else if (command.payload == "erase") tool = TileTool::Erase;
        else if (command.payload == "rect") tool = TileTool::Rect;
        else if (command.payload == "line") tool = TileTool::Line;
        else if (command.payload == "picker") tool = TileTool::Picker;
        m_tilePaint->setTool(tool);
        return true;
    }

    if (command.name == "tilemap.brush") {
        if (!m_tilePaint) return false;
        try {
            const nlohmann::json payload = nlohmann::json::parse(command.payload);
            TileBrush brush;
            brush.w = payload.value("w", 1);
            brush.h = payload.value("h", 1);
            brush.gids.clear();
            if (payload.contains("gids") && payload["gids"].is_array()) {
                for (const auto& gid : payload["gids"]) {
                    brush.gids.push_back(gid.get<dodoe::UInt32>());
                }
            }
            if (brush.w <= 0) brush.w = 1;
            if (brush.h <= 0) brush.h = 1;
            if (brush.gids.size() < static_cast<std::size_t>(brush.w * brush.h)) {
                brush.gids.assign(static_cast<std::size_t>(brush.w * brush.h), 0);
            }
            m_tilePaint->setBrush(brush);
        } catch (const nlohmann::json::exception&) {
            return false;
        }
        return true;
    }

    if (command.name == "tilemap.layer_active") {
        if (!m_tilePaint) return false;
        const std::uint64_t uuid = command.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
        if (uuid == 0) return false;
        m_tilePaint->setActiveLayer(dodoe::UUID(uuid));
        return true;
    }

    if (command.name == "tilemap.layer_visible") {
        std::uint64_t uuid = 0;
        int visible = 0;
        if (std::sscanf(command.payload.c_str(), "%llu,%d", &uuid, &visible) >= 2) {
            executeTilemapLayerField(dodoe::UUID(uuid), "visible", visible != 0);
            return true;
        }
        return false;
    }

    if (command.name == "tilemap.layer_opacity") {
        std::uint64_t uuid = 0;
        float opacity = 1.0f;
        if (std::sscanf(command.payload.c_str(), "%llu,%f", &uuid, &opacity) >= 2) {
            executeTilemapLayerField(dodoe::UUID(uuid), "opacity", opacity);
            return true;
        }
        return false;
    }

    if (command.name == "tilemap.layer_rename") {
        if (!m_session) return false;
        const std::size_t comma = command.payload.find(',');
        if (comma == std::string::npos) return false;
        const std::uint64_t uuid = std::strtoull(command.payload.substr(0, comma).c_str(), nullptr, 10);
        const std::string name = command.payload.substr(comma + 1);
        if (uuid == 0 || name.empty()) return false;
        return m_session->renameEntity(uuid, name);
    }

    if (command.name == "tilemap.layer_add") {
        if (!m_session || !m_tilePaint) return false;
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (!tilemapUuid.isValid()) return false;
        const std::string name = command.payload.empty() ? std::string("Layer") : command.payload;
        dodoe::World* world = runtimeWorld();
        dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
        dodoe::Entity tm = scene ? ResolveEntity(scene, tilemapUuid) : dodoe::Entity();
        if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;
        const auto& comp = tm.getComponent<TilemapComponent>();
        auto command2 = std::make_unique<CreateTileLayerCommand>(
            tilemapUuid, dodoe::String(name.c_str()), comp.map_width, comp.map_height);
        auto* executed = m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        if (executed) {
            const dodoe::UUID layerUuid = static_cast<CreateTileLayerCommand*>(executed)->created();
            if (layerUuid.isValid()) {
                m_tilePaint->setActiveLayer(layerUuid);
            }
        }
        return true;
    }

    if (command.name == "tilemap.layer_move") {
        if (!m_session || !m_tilePaint) return false;
        const std::size_t comma = command.payload.find(',');
        if (comma == std::string::npos) return false;
        const std::uint64_t uuid = std::strtoull(command.payload.substr(0, comma).c_str(), nullptr, 10);
        const std::string dir = command.payload.substr(comma + 1);
        if (uuid == 0) return false;
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (!tilemapUuid.isValid()) return false;
        auto command2 = std::make_unique<ReorderTileLayerCommand>(
            tilemapUuid, dodoe::UUID(uuid), dir == "up");
        m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        return true;
    }

    if (command.name == "tilemap.layer_remove") {
        if (!m_session || !m_tilePaint) return false;
        const std::uint64_t uuid = command.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
        if (uuid == 0) return false;
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (tilemapUuid.isValid()) {
            EditorEntity* tilemapEntity =
                m_session->documentModel().findEntity(static_cast<std::uint64_t>(tilemapUuid));
            if (tilemapEntity) {
                for (auto& component : tilemapEntity->nativeComponents) {
                    if (component.typeName != "TilemapComponent" ||
                        !component.value.contains("layer_order")) {
                        continue;
                    }
                    auto& order = component.value["layer_order"];
                    order.erase(std::remove(order.begin(), order.end(), uuid), order.end());
                    break;
                }
            }
        }
        m_session->deleteEntity(uuid);
        return true;
    }

    if (command.name == "tilemap.add_tileset") {
        if (!m_session || !m_tilePaint) return false;
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (!tilemapUuid.isValid()) return false;
        try {
            const nlohmann::json payload = nlohmann::json::parse(command.payload);
            const std::string image = payload.value("image", std::string());
            if (image.empty()) return false;
            const dodoe::UInt32 tw = payload.value("tile_width", 16u);
            const dodoe::UInt32 th = payload.value("tile_height", 16u);
            const dodoe::UInt32 margin = payload.value("margin", 0u);
            const dodoe::UInt32 spacing = payload.value("spacing", 0u);
            auto command2 = std::make_unique<CreateTilesetCommand>(
                tilemapUuid, dodoe::String(image.c_str()), tw, th, margin, spacing);
            m_session->history().execute(std::move(command2), m_session->documentModel());
            m_session->notifyDocumentChanged();
        } catch (const nlohmann::json::exception&) {
            return false;
        }
        return true;
    }

    if (command.name == "tilemap.resize") {
        if (!m_session) return false;
        std::uint64_t uuid = 0;
        unsigned int w = 0, h = 0;
        if (std::sscanf(command.payload.c_str(), "%llu,%u,%u", &uuid, &w, &h) >= 3) {
            if (uuid == 0 || w == 0 || h == 0) return false;
            auto command2 = std::make_unique<ResizeTilemapCommand>(dodoe::UUID(uuid), w, h);
            m_session->history().execute(std::move(command2), m_session->documentModel());
            m_session->notifyDocumentChanged();
            return true;
        }
        return false;
    }

    if (command.name == "tilemap.remove_tileset") {
        if (!m_session || !m_tilePaint) return false;
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (!tilemapUuid.isValid()) return false;
        const std::uint64_t assetId = command.payload.empty()
            ? 0
            : static_cast<std::uint64_t>(std::strtoull(command.payload.c_str(), nullptr, 10));
        if (assetId == 0) return false;
        auto command2 = std::make_unique<RemoveTilesetCommand>(tilemapUuid, dodoe::UUID(assetId));
        m_session->history().execute(std::move(command2), m_session->documentModel());
        m_session->notifyDocumentChanged();
        return true;
    }

    return false;
}

bool RuntimeEditorBackend::queryTilemapState(const std::string& tilemapUuid, nlohmann::json& out) const
{
    out = nullptr;
    if (!m_booted || !m_app || !m_tilePaint) return false;

    dodoe::UUID activeUuid = m_tilePaint->activeTilemap();
    if (!tilemapUuid.empty()) {
        const std::uint64_t parsed = std::strtoull(tilemapUuid.c_str(), nullptr, 10);
        if (parsed != 0) activeUuid = dodoe::UUID(parsed);
    }
    if (!activeUuid.isValid()) return false;

    dodoe::World* world = runtimeWorld();
    dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return false;
    dodoe::Entity tm = ResolveEntity(scene, activeUuid);
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;

    const auto& comp = tm.getComponent<TilemapComponent>();

    nlohmann::json tilemap;
    tilemap["uuid"] = static_cast<std::uint64_t>(activeUuid);
    tilemap["name"] = tm.name().c_str();
    tilemap["map_width"] = comp.map_width;
    tilemap["map_height"] = comp.map_height;
    tilemap["tile_width"] = comp.tile_width;
    tilemap["tile_height"] = comp.tile_height;

    nlohmann::json layers = nlohmann::json::array();
    if (tm.hasComponent<HierarchyComponent>()) {
        for (auto& child : tm.getComponent<HierarchyComponent>().children) {
            if (!child.valid() || !child.hasComponent<TileLayerComponent>()) continue;
            const auto& layer = child.getComponent<TileLayerComponent>();
            nlohmann::json item;
            item["uuid"] = static_cast<std::uint64_t>(child.uuid());
            item["name"] = child.name().c_str();
            item["visible"] = layer.visible;
            item["opacity"] = layer.opacity;
            item["active"] = child.uuid() == m_tilePaint->activeLayer();
            layers.push_back(std::move(item));
        }
    }

    nlohmann::json tilesets = nlohmann::json::array();
    for (const auto& ref : comp.tilesets) {
        const dodoe::Tileset* tileset = ref.get();
        if (!tileset) continue;
        nlohmann::json item;
        item["asset_id"] = static_cast<std::uint64_t>(ref.getObjectID().asset_id);
        item["name"] = tileset->name.c_str();
        item["image_path"] = tileset->image_path.c_str();
        item["tile_width"] = tileset->tile_width;
        item["tile_height"] = tileset->tile_height;
        item["columns"] = tileset->columns;
        item["tile_count"] = tileset->tile_count;
        item["first_gid"] = tileset->first_gid;
        tilesets.push_back(std::move(item));
    }

    nlohmann::json brush;
    brush["w"] = m_tilePaint->brush().w;
    brush["h"] = m_tilePaint->brush().h;
    brush["gids"] = nlohmann::json::array();
    for (dodoe::UInt32 gid : m_tilePaint->brush().gids) {
        brush["gids"].push_back(gid);
    }

    out["tilemap"] = std::move(tilemap);
    out["active_layer"] = static_cast<std::uint64_t>(m_tilePaint->activeLayer());
    out["layers"] = std::move(layers);
    out["tilesets"] = std::move(tilesets);
    out["tool"] = TileToolName(m_tilePaint->tool());
    out["brush"] = std::move(brush);
    return true;
}

void RuntimeEditorBackend::emitTilemapEditMode(bool active)
{
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"tilemap_edit_mode", active ? "1" : "0"});
    }
}

void RuntimeEditorBackend::updateTileEditFromSelection()
{
    if (!m_tilePaint || m_selectedUuid == 0) {
        emitTilemapEditMode(false);
        return;
    }
    dodoe::World* world = runtimeWorld();
    dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) {
        emitTilemapEditMode(false);
        return;
    }
    dodoe::Entity entity = scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
    if (!entity.valid()) {
        emitTilemapEditMode(false);
        return;
    }
    if (entity.hasComponent<TilemapComponent>()) {
        const dodoe::UUID tilemapUuid = entity.uuid();
        m_tilePaint->setActiveEntity(tilemapUuid);
        activateTilemapEdit(tilemapUuid, m_tilePaint->activeLayer());
    } else if (entity.hasComponent<TileLayerComponent>()) {
        const dodoe::UUID layerUuid = entity.uuid();
        m_tilePaint->setActiveEntity(layerUuid);
        const dodoe::UUID tilemapUuid = m_tilePaint->activeTilemap();
        if (tilemapUuid.isValid()) {
            activateTilemapEdit(tilemapUuid, layerUuid);
        } else {
            emitTilemapEditMode(false);
        }
    } else {
        emitTilemapEditMode(false);
    }
}

void RuntimeEditorBackend::activateTilemapEdit(const dodoe::UUID& tilemapUuid, const dodoe::UUID& layerUuid)
{
    if (layerUuid.isValid()) {
        m_tilePaint->setActiveLayer(layerUuid);
    }
    if (m_camera) {
        dodoe::World* world = runtimeWorld();
        dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
        dodoe::Entity tm = scene ? ResolveEntity(scene, tilemapUuid) : dodoe::Entity();
        if (tm.valid() && tm.hasComponent<TilemapComponent>()) {
            const auto& comp = tm.getComponent<TilemapComponent>();
            const dodoe::Vector3f center{
                static_cast<float>(comp.map_width * comp.tile_width) * 0.5f,
                static_cast<float>(comp.map_height * comp.tile_height) * 0.5f,
                0.0f};
            const float radius = std::max(static_cast<float>(comp.map_width * comp.tile_width),
                                          static_cast<float>(comp.map_height * comp.tile_height)) * 0.5f;
            m_camera->focusOn(center, radius);
            m_camera->setMode(EditorCamera::Mode::Ortho2D);
        }
    }
    if (m_eventCallback) {
        m_eventCallback(BackendEventMessage{"camera_mode_changed", "2d"});
    }
    emitTilemapEditMode(true);
}

bool RuntimeEditorBackend::screenToCell(float screenX, float screenY, int& outX, int& outY) const
{
    if (!m_camera || !m_tilePaint || !m_tilePaint->hasTarget()) return false;
    dodoe::World* world = runtimeWorld();
    dodoe::Scene* scene = world ? world->getActiveScene() : nullptr;
    if (!scene) return false;
    dodoe::Entity tm = ResolveEntity(scene, m_tilePaint->activeTilemap());
    if (!tm.valid() || !tm.hasComponent<TilemapComponent>()) return false;

    dodoe::Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    if (std::abs(dir.z) < 1e-6f) return false;
    const float t = -origin.z / dir.z;
    if (t < 0.0f) return false;
    const dodoe::Vector3f worldPos = origin + dir * t;
    return TileCoord::worldToCell(tm.getComponent<TilemapComponent>(),
                                  dodoe::Matrix4f(1.0f), worldPos, outX, outY);
}

void RuntimeEditorBackend::executeTilemapLayerField(dodoe::UUID layer, const std::string& field,
                                                    const nlohmann::json& value)
{
    if (!m_session) return;
    EditorEntity* entity = m_session->documentModel().findEntity(static_cast<std::uint64_t>(layer));
    if (!entity) return;
    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName != "TileLayerComponent") continue;
        auto command = std::make_unique<SetFieldValueCommand>(
            static_cast<std::uint64_t>(layer), i, field, value);
        m_session->history().execute(std::move(command), m_session->documentModel());
        m_session->notifyDocumentChanged();
        return;
    }
}

} // namespace cakery
