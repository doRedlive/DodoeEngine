// do@Redlive

#include "CreateTileLayerCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/log/log_system.h"

#include <algorithm>

namespace cakery {

CreateTileLayerCommand::CreateTileLayerCommand(dodoe::UUID tilemap, dodoe::String name,
                                               dodoe::UInt32 width, dodoe::UInt32 height)
    : m_tilemap(tilemap)
    , m_name(std::move(name))
    , m_width(width)
    , m_height(height)
{}

bool CreateTileLayerCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid()) return false;

    // redo 时沿用记录 uuid，保证 undo/redo 后实体身份稳定
    dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
    dodoe::Entity layerEntity = scene->createEntity(uuid, dodoe::String(m_name.c_str()));
    if (!layerEntity.valid()) return false;
    m_createdUuid = layerEntity.uuid();

    auto& layer = layerEntity.addComponent<dodoe::TileLayerComponent>();
    layer.layer_name = m_name;
    layer.resize(m_width, m_height);

    // 按 AttachChild 权威模式挂接到 tilemap
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

    LOG_INFO("[CreateTileLayer] {} ({})", m_name, static_cast<uint64_t>(m_createdUuid));
    return true;
}

void CreateTileLayerCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto layerEntity = ResolveEntity(scene, m_createdUuid);
    if (!layerEntity.valid()) return;

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

std::string CreateTileLayerCommand::label() const
{
    return std::string("Create Layer ") + m_name.c_str();
}

} // namespace cakery
