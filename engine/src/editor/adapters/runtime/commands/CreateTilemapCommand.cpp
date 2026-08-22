// do@Redlive

#include "CreateTilemapCommand.h"

#include "CreateTileLayerCommand.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
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

CreateTilemapCommand::CreateTilemapCommand(dodoe::String name, dodoe::UInt32 width, dodoe::UInt32 height,
                                           dodoe::UInt32 tileWidth, dodoe::UInt32 tileHeight)
    : m_name(std::move(name))
    , m_width(width)
    , m_height(height)
    , m_tileWidth(tileWidth)
    , m_tileHeight(tileHeight)
{}

void CreateTilemapCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return;

    dodoe::Entity tilemapEntity = ResolveEntity(scene, m_createdUuid);
    if (!tilemapEntity.valid()) {
        dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
        tilemapEntity = scene->createEntity(uuid, m_name);
        if (!tilemapEntity.valid()) return;
        m_createdUuid = tilemapEntity.uuid();

        auto& tm = tilemapEntity.addComponent<dodoe::TilemapComponent>();
        tm.map_width = m_width;
        tm.map_height = m_height;
        tm.tile_width = m_tileWidth;
        tm.tile_height = m_tileHeight;
        tm.dirty = true;

        if (!tilemapEntity.hasComponent<dodoe::HierarchyComponent>()) {
            tilemapEntity.addComponent<dodoe::HierarchyComponent>();
        }
    }

    dodoe::Entity layerEntity = m_layerUuid.isValid() ? ResolveEntity(scene, m_layerUuid) : dodoe::Entity();
    if (!layerEntity.valid()) {
        CreateTileLayerCommand layerCmd(m_createdUuid, "Layer 1", m_width, m_height);
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

    if (!model.findEntity(static_cast<std::uint64_t>(m_createdUuid))) {
        model.createEntity(std::string(m_name.c_str()), static_cast<std::uint64_t>(m_createdUuid));
        nlohmann::json value;
        value["map_width"] = m_width;
        value["map_height"] = m_height;
        value["tile_width"] = m_tileWidth;
        value["tile_height"] = m_tileHeight;
        value["tilesets"] = nlohmann::json::array();
        value["dirty"] = true;
        model.addComponent(static_cast<std::uint64_t>(m_createdUuid),
                           EditorComponent{"TilemapComponent", std::move(value)});
    }
}

void CreateTilemapCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_createdUuid);

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

std::string CreateTilemapCommand::label() const
{
    return std::string("Create Tilemap ") + m_name.c_str();
}

} // namespace cakery
