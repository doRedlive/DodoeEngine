// do@Redlive

#include "CreateTilemapWithTilesetCommand.h"

#include "CreateTileLayerCommand.h"
#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/resource_manager.h"

#include <algorithm>
#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

} // namespace

CreateTilemapWithTilesetCommand::CreateTilemapWithTilesetCommand(dodoe::String name,
                                                                 dodoe::UUID tilesetAssetId,
                                                                 nlohmann::json position)
    : m_name(std::move(name))
    , m_tilesetAssetId(tilesetAssetId)
    , m_position(std::move(position))
{
}

void CreateTilemapWithTilesetCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) {
        return;
    }

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) {
        return;
    }

    dodoe::Tileset* tileset = resourceManager.loadObject<dodoe::Tileset>(m_tilesetAssetId, 0);
    if (!tileset) {
        return;
    }
    const dodoe::UInt32 tileW = tileset->tile_width > 0 ? tileset->tile_width : 16;
    const dodoe::UInt32 tileH = tileset->tile_height > 0 ? tileset->tile_height : 16;
    const dodoe::UInt32 mapW = 20;
    const dodoe::UInt32 mapH = 20;

    dodoe::Entity tilemapEntity = ResolveEntity(scene, m_createdUuid);
    if (!tilemapEntity.valid()) {
        dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
        tilemapEntity = scene->createEntity(uuid, m_name);
        if (!tilemapEntity.valid()) {
            return;
        }
        m_createdUuid = tilemapEntity.uuid();

        auto& tm = tilemapEntity.addComponent<dodoe::TilemapComponent>();
        tm.map_width = mapW;
        tm.map_height = mapH;
        tm.tile_width = tileW;
        tm.tile_height = tileH;
        tm.dirty = true;

        if (!tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
            tilemapEntity.addComponent<dodoe::HierarchyComponent>();
        }

        if (tilemapEntity.hasComponent<dodoe::TransformComponent>()) {
            auto& tc = tilemapEntity.getComponent<dodoe::TransformComponent>();
            if (m_position.is_array() && m_position.size() >= 3) {
                tc.position = dodoe::Vector3f(m_position[0].get<float>(),
                                              m_position[1].get<float>(),
                                              m_position[2].get<float>());
            }
            tc.dirty = true;
        }
    }

    if (!model.findEntity(static_cast<std::uint64_t>(m_createdUuid))) {
        model.createEntity(std::string(m_name.c_str()), static_cast<std::uint64_t>(m_createdUuid));
        nlohmann::json value;
        value["map_width"] = mapW;
        value["map_height"] = mapH;
        value["tile_width"] = tileW;
        value["tile_height"] = tileH;
        value["tilesets"] = nlohmann::json::array();
        value["dirty"] = true;
        model.addComponent(static_cast<std::uint64_t>(m_createdUuid),
                           EditorComponent{"TilemapComponent", std::move(value)});
        nlohmann::json transform;
        transform["position"] = m_position;
        transform["rotation"] = nlohmann::json::array({0.0, 0.0, 0.0});
        transform["scale"] = nlohmann::json::array({1.0, 1.0, 1.0});
        model.updateComponent(static_cast<std::uint64_t>(m_createdUuid), 2, std::move(transform));
    }

    dodoe::Entity layerEntity = m_layerUuid.isValid() ? ResolveEntity(scene, m_layerUuid) : dodoe::Entity();
    if (!layerEntity.valid()) {
        CreateTileLayerCommand layerCmd(m_createdUuid, "Layer 1", mapW, mapH);
        if (m_layerUuid.isValid()) {
            layerCmd.setCreatedUuid(m_layerUuid);
        }
        layerCmd.execute(model);
        m_layerUuid = layerCmd.created();
        if (!m_layerUuid.isValid()) {
            scene->destroyEntity(tilemapEntity);
            return;
        }
    }

    if (!m_created) {
        dodoe::Entity tmEntity = ResolveEntity(scene, m_createdUuid);
        if (tmEntity.valid() && tmEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tmEntity.getComponent<dodoe::TilemapComponent>();
            tm.tilesets.push_back(dodoe::PPtr<dodoe::Tileset>(tileset));
            tm.dirty = true;
        }
        if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_createdUuid)) {
            tilesets->push_back(nlohmann::json{
                {"asset_id", static_cast<std::uint64_t>(m_tilesetAssetId)},
                {"sub_object_id", 0},
            });
        }
        m_created = true;
    }
}

void CreateTilemapWithTilesetCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_createdUuid);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            auto& tilesets = tm.tilesets;
            tilesets.erase(
                std::remove_if(tilesets.begin(), tilesets.end(), [this](const dodoe::PPtr<dodoe::Tileset>& ref) {
                    return ref.getObjectID().asset_id == m_tilesetAssetId;
                }),
                tilesets.end());
            tm.dirty = true;
        }

        auto layerEntity = ResolveEntity(scene, m_layerUuid);
        if (layerEntity.valid()) {
            if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
                auto& parentHC = tilemapEntity.getComponent<dodoe::HierarchyComponent>();
                auto& children = parentHC.children;
                children.erase(std::remove(children.begin(), children.end(), layerEntity), children.end());
                parentHC.child_count = static_cast<int>(children.size());
                parentHC.dirty = true;
            }
            scene->destroyEntity(layerEntity);
        }

        if (tilemapEntity.valid()) {
            scene->destroyEntity(tilemapEntity);
        }
    }

    model.deleteEntity(static_cast<std::uint64_t>(m_layerUuid));
    model.deleteEntity(static_cast<std::uint64_t>(m_createdUuid));
}

std::string CreateTilemapWithTilesetCommand::label() const
{
    return std::string("Create Tilemap ") + m_name.c_str();
}

} // namespace cakery
