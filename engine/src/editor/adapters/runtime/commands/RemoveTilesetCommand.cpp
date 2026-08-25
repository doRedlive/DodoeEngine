// do@Redlive

#include "RemoveTilesetCommand.h"

#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
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

RemoveTilesetCommand::RemoveTilesetCommand(dodoe::UUID tilemap, dodoe::UUID assetId)
    : m_tilemap(tilemap)
    , m_assetId(assetId)
{}

void RemoveTilesetCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_tilemap);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            auto& tilesets = tm.tilesets;
            tilesets.erase(
                std::remove_if(tilesets.begin(), tilesets.end(), [this](const dodoe::PPtr<dodoe::Tileset>& ref) {
                    return ref.getObjectID().asset_id == m_assetId;
                }),
                tilesets.end());
            tm.dirty = true;
        }
    }

    if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_tilemap)) {
        tilesets->erase(
            std::remove_if(tilesets->begin(), tilesets->end(), [this](const nlohmann::json& item) {
                return item.contains("asset_id") &&
                       item["asset_id"].get<std::uint64_t>() == static_cast<std::uint64_t>(m_assetId);
            }),
            tilesets->end());
    }
    m_removed = true;
}

void RemoveTilesetCommand::revert(EditorDocumentModel& model)
{
    if (!m_removed) return;

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_tilemap);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            if (dodoe::Tileset* tileset = resourceManager.loadObject<dodoe::Tileset>(m_assetId, 0)) {
                tm.tilesets.push_back(dodoe::PPtr<dodoe::Tileset>(tileset));
            }
            tm.dirty = true;
        }
    }

    if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_tilemap)) {
        tilesets->push_back(nlohmann::json{
            {"asset_id", static_cast<std::uint64_t>(m_assetId)},
            {"sub_object_id", 0},
        });
    }
}

std::string RemoveTilesetCommand::label() const
{
    return std::string("Remove Tileset (") + std::to_string(static_cast<std::uint64_t>(m_assetId)) + ")";
}

} // namespace cakery
