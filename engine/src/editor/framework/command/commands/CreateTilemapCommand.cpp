// do@Redlive

#include "CreateTilemapCommand.h"
#include "CreateTileLayerCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/log/log_system.h"

#include <algorithm>

namespace cakery {

CreateTilemapCommand::CreateTilemapCommand(dodoe::String name, dodoe::UInt32 width, dodoe::UInt32 height,
                                           dodoe::UInt32 tileWidth, dodoe::UInt32 tileHeight)
    : m_name(std::move(name))
    , m_width(width)
    , m_height(height)
    , m_tileWidth(tileWidth)
    , m_tileHeight(tileHeight)
{}

bool CreateTilemapCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    // redo 时沿用记录 uuid，保证 undo/redo 后实体身份稳定
    dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
    dodoe::Entity tilemapEntity = scene->createEntity(uuid, m_name);
    if (!tilemapEntity.valid()) return false;
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

    // 自动创建默认图层
    if (!m_layerUuid.isValid()) {
        CreateTileLayerCommand layerCmd(m_createdUuid, "Layer 1", m_width, m_height);
        if (!layerCmd.execute(ctx)) {
            scene->destroyEntity(tilemapEntity);
            return false;
        }
        m_layerUuid = layerCmd.created();
    }

    LOG_INFO("[CreateTilemap] {} ({})", m_name.c_str(), static_cast<uint64_t>(m_createdUuid));
    return true;
}

void CreateTilemapCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto tilemapEntity = ResolveEntity(scene, m_createdUuid);

    // 先销毁默认图层（从父 children 移除并更新 child_count）
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

std::string CreateTilemapCommand::label() const
{
    return std::string("Create Tilemap ") + m_name.c_str();
}

} // namespace cakery
