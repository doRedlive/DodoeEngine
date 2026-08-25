// do@Redlive

#include "ResizeTilemapCommand.h"

#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <algorithm>
#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

} // namespace

ResizeTilemapCommand::ResizeTilemapCommand(dodoe::UUID tilemap, dodoe::UInt32 newWidth, dodoe::UInt32 newHeight)
    : m_tilemap(tilemap)
    , m_newWidth(newWidth)
    , m_newHeight(newHeight)
{}

void ResizeTilemapCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::TilemapComponent>()) return;

    auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
    m_oldWidth = tm.map_width;
    m_oldHeight = tm.map_height;

    if (!m_done) {
        if (tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
            for (auto& child : tilemapEntity.getComponent<dodoe::HierarchyComponent>().children) {
                if (!child.valid() || !child.hasComponent<dodoe::TileLayerComponent>()) continue;
                auto& layer = child.getComponent<dodoe::TileLayerComponent>();
                LayerSnapshot snapshot;
                snapshot.uuid = child.uuid();
                snapshot.width = layer.layer_width;
                snapshot.height = layer.layer_height;
                snapshot.tiles.assign(layer.tiles.begin(), layer.tiles.end());
                m_layers.push_back(std::move(snapshot));
            }
        }
        m_done = true;
    }

    applyDims(model, m_newWidth, m_newHeight);
}

void ResizeTilemapCommand::revert(EditorDocumentModel& model)
{
    if (!m_done) return;

    auto* scene = ActiveScene();
    if (!scene) return;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::TilemapComponent>()) return;

    auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
    tm.map_width = m_oldWidth;
    tm.map_height = m_oldHeight;
    tm.dirty = true;

    if (tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
        for (auto& child : tilemapEntity.getComponent<dodoe::HierarchyComponent>().children) {
            if (!child.valid() || !child.hasComponent<dodoe::TileLayerComponent>()) continue;
            auto& layer = child.getComponent<dodoe::TileLayerComponent>();
            const auto it = std::find_if(m_layers.begin(), m_layers.end(), [&](const LayerSnapshot& snapshot) {
                return snapshot.uuid == child.uuid();
            });
            if (it == m_layers.end()) continue;
            layer.resize(it->width, it->height);
            layer.tiles.assign(it->tiles.begin(), it->tiles.end());
        }
    }

    if (nlohmann::json* value = FindTilemapComponentValue(model, m_tilemap)) {
        (*value)["map_width"] = m_oldWidth;
        (*value)["map_height"] = m_oldHeight;
    }
    for (const LayerSnapshot& snapshot : m_layers) {
        EditorEntity* entity = model.findEntity(static_cast<std::uint64_t>(snapshot.uuid));
        if (!entity) continue;
        for (auto& component : entity->nativeComponents) {
            if (component.typeName != "TileLayerComponent") continue;
            component.value["layer_width"] = snapshot.width;
            component.value["layer_height"] = snapshot.height;
            nlohmann::json& tiles = component.value["tiles"];
            tiles = nlohmann::json::array();
            for (dodoe::UInt32 gid : snapshot.tiles) {
                tiles.push_back(gid);
            }
            break;
        }
    }
}

void ResizeTilemapCommand::applyDims(EditorDocumentModel& model, dodoe::UInt32 width, dodoe::UInt32 height)
{
    auto* scene = ActiveScene();
    if (!scene) return;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::TilemapComponent>()) return;

    auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
    tm.map_width = width;
    tm.map_height = height;
    tm.dirty = true;

    if (tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
        for (auto& child : tilemapEntity.getComponent<dodoe::HierarchyComponent>().children) {
            if (!child.valid() || !child.hasComponent<dodoe::TileLayerComponent>()) continue;
            child.getComponent<dodoe::TileLayerComponent>().resize(width, height);
        }
    }

    if (nlohmann::json* value = FindTilemapComponentValue(model, m_tilemap)) {
        (*value)["map_width"] = width;
        (*value)["map_height"] = height;
    }
    for (const LayerSnapshot& snapshot : m_layers) {
        EditorEntity* entity = model.findEntity(static_cast<std::uint64_t>(snapshot.uuid));
        if (!entity) continue;
        for (auto& component : entity->nativeComponents) {
            if (component.typeName != "TileLayerComponent") continue;
            component.value["layer_width"] = width;
            component.value["layer_height"] = height;
            std::vector<dodoe::UInt32> next(static_cast<std::size_t>(width) * height, 0);
            const dodoe::UInt32 cw = std::min(width, snapshot.width);
            const dodoe::UInt32 ch = std::min(height, snapshot.height);
            for (dodoe::UInt32 yy = 0; yy < ch; ++yy) {
                for (dodoe::UInt32 xx = 0; xx < cw; ++xx) {
                    next[static_cast<std::size_t>(yy) * width + xx] =
                        snapshot.tiles[static_cast<std::size_t>(yy) * snapshot.width + xx];
                }
            }
            nlohmann::json& tiles = component.value["tiles"];
            tiles = nlohmann::json::array();
            for (dodoe::UInt32 gid : next) {
                tiles.push_back(gid);
            }
            break;
        }
    }
}

std::string ResizeTilemapCommand::label() const
{
    return std::string("Resize Tilemap to ") + std::to_string(m_newWidth) + "x" + std::to_string(m_newHeight);
}

} // namespace cakery
