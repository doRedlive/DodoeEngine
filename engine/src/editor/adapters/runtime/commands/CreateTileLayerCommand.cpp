// do@Redlive

#include "CreateTileLayerCommand.h"

#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
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

CreateTileLayerCommand::CreateTileLayerCommand(dodoe::UUID tilemap, dodoe::String name,
                                               dodoe::UInt32 width, dodoe::UInt32 height)
    : m_tilemap(tilemap)
    , m_name(std::move(name))
    , m_width(width)
    , m_height(height)
{}

void CreateTileLayerCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return;

    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid()) return;

    dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
    dodoe::Entity layerEntity = scene->createEntity(uuid, dodoe::String(m_name.c_str()));
    if (!layerEntity.valid()) return;
    m_createdUuid = layerEntity.uuid();

    auto& layer = layerEntity.addComponent<dodoe::TileLayerComponent>();
    layer.layer_name = m_name;
    layer.resize(m_width, m_height);

    if (!layerEntity.hasComponent<dodoe::HierarchyComponent>()) {
        layerEntity.addComponent<dodoe::HierarchyComponent>();
    }
    if (!tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
        tilemapEntity.addComponent<dodoe::HierarchyComponent>();
    }

    auto& childHC = layerEntity.getComponent<dodoe::HierarchyComponent>();
    childHC.parent = tilemapEntity;
    childHC.parent_uuid = tilemapEntity.uuid();
    childHC.dirty = true;

    auto& parentHC = tilemapEntity.getComponent<dodoe::HierarchyComponent>();
    const auto child_it = std::find(parentHC.children.begin(), parentHC.children.end(), layerEntity);
    if (child_it == parentHC.children.end()) {
        parentHC.children.push_back(layerEntity);
    }
    parentHC.child_count = static_cast<int>(parentHC.children.size());
    parentHC.dirty = true;

    if (!model.findEntity(static_cast<std::uint64_t>(m_createdUuid))) {
        model.createEntity(std::string(m_name.c_str()), static_cast<std::uint64_t>(m_createdUuid));
        nlohmann::json value;
        value["layer_name"] = m_name.c_str();
        value["layer_width"] = m_width;
        value["layer_height"] = m_height;
        value["tiles"] = nlohmann::json::array();
        for (dodoe::UInt32 i = 0; i < m_width * m_height; ++i) {
            value["tiles"].push_back(0);
        }
        value["visible"] = true;
        value["opacity"] = 1.0;
        value["offset_x"] = 0;
        value["offset_y"] = 0;
        model.addComponent(static_cast<std::uint64_t>(m_createdUuid),
                           EditorComponent{"TileLayerComponent", std::move(value)});
        model.reparentEntity(static_cast<std::uint64_t>(m_createdUuid),
                             static_cast<std::uint64_t>(m_tilemap));
        if (nlohmann::json* order = FindTilemapLayerOrderArray(model, m_tilemap)) {
            order->push_back(static_cast<std::uint64_t>(m_createdUuid));
        }
    }
}

void CreateTileLayerCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto layerEntity = ResolveEntity(scene, m_createdUuid);
        if (layerEntity.valid()) {
            auto tilemapEntity = ResolveEntity(scene, m_tilemap);
            if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
                auto& parentHC = tilemapEntity.getComponent<dodoe::HierarchyComponent>();
                auto& children = parentHC.children;
                children.erase(std::remove(children.begin(), children.end(), layerEntity), children.end());
                parentHC.child_count = static_cast<int>(children.size());
                parentHC.dirty = true;
            }
            scene->destroyEntity(layerEntity);
        }
    }

    if (nlohmann::json* order = FindTilemapLayerOrderArray(model, m_tilemap)) {
        order->erase(std::remove(order->begin(), order->end(),
                                 static_cast<std::uint64_t>(m_createdUuid)),
                     order->end());
    }
    model.deleteEntity(static_cast<std::uint64_t>(m_createdUuid));
}

std::string CreateTileLayerCommand::label() const
{
    return std::string("Create Layer ") + m_name.c_str();
}

} // namespace cakery
