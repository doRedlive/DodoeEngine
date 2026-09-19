// do@Redlive

#include "UpdateTilesetCommand.h"

#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/asset/types/tileset_asset.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#include <filesystem>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

dodoe::Tileset* FindTileset(dodoe::Entity& tilemapEntity, dodoe::UUID assetId) {
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::TilemapComponent>()) return nullptr;
    auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
    for (auto& ref : tm.tilesets) {
        if (ref.getObjectID().asset_id == assetId) {
            return ref.get();
        }
    }
    return nullptr;
}

} // namespace

UpdateTilesetCommand::UpdateTilesetCommand(dodoe::UUID tilemap, dodoe::UUID tilesetAssetId,
                                           dodoe::UInt32 tileWidth, dodoe::UInt32 tileHeight,
                                           dodoe::UInt32 margin, dodoe::UInt32 spacing)
    : m_tilemap(tilemap)
    , m_tilesetAssetId(tilesetAssetId)
{
    m_params.tile_width = tileWidth;
    m_params.tile_height = tileHeight;
    m_params.margin = margin;
    m_params.spacing = spacing;
}

bool UpdateTilesetCommand::execute(EditorDocumentModel& model)
{
    (void)model;
    auto* scene = ActiveScene();
    if (!scene) return false;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    dodoe::Tileset* tileset = FindTileset(tilemapEntity, m_tilesetAssetId);
    if (!tileset) return false;

    if (!m_captured) {
        m_previous.tile_width = tileset->tile_width;
        m_previous.tile_height = tileset->tile_height;
        m_previous.columns = tileset->columns;
        m_previous.tile_count = tileset->tile_count;
        m_previous.margin = tileset->margin;
        m_previous.spacing = tileset->spacing;
        m_captured = true;
    }

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) return false;

    dodoe::UInt32 columns = 0;
    dodoe::UInt32 rows = 0;
    const std::filesystem::path imagePath = std::filesystem::path(tileset->image_path.c_str());
    const std::filesystem::path resolved = imagePath.is_absolute()
        ? imagePath
        : assetManager->getAssetDir() / imagePath;
    auto* texture = resourceManager.loadObjectByPath<dodoe::Texture2D>(
        dodoe::FileID(dodoe::String(resolved.generic_string().c_str())));
    if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
        const dodoe::UInt32 stepW = m_params.tile_width + m_params.spacing;
        const dodoe::UInt32 stepH = m_params.tile_height + m_params.spacing;
        const dodoe::Int32 availW =
            texture->getWidth() - static_cast<dodoe::Int32>(m_params.margin * 2);
        const dodoe::Int32 availH =
            texture->getHeight() - static_cast<dodoe::Int32>(m_params.margin * 2);
        if (stepW > 0 && stepH > 0 && availW >= static_cast<dodoe::Int32>(m_params.tile_width) &&
            availH >= static_cast<dodoe::Int32>(m_params.tile_height)) {
            columns = static_cast<dodoe::UInt32>((availW + static_cast<dodoe::Int32>(m_params.spacing)) /
                                                 static_cast<dodoe::Int32>(stepW));
            rows = static_cast<dodoe::UInt32>((availH + static_cast<dodoe::Int32>(m_params.spacing)) /
                                              static_cast<dodoe::Int32>(stepH));
        }
    }
    if (columns == 0 || rows == 0) {
        columns = m_previous.columns;
        rows = m_previous.tile_count / (m_previous.columns == 0 ? 1 : m_previous.columns);
    }

    State next;
    next.tile_width = m_params.tile_width;
    next.tile_height = m_params.tile_height;
    next.margin = m_params.margin;
    next.spacing = m_params.spacing;
    next.columns = columns;
    next.tile_count = columns * rows;
    return apply(next);
}

bool UpdateTilesetCommand::revert(EditorDocumentModel& model)
{
    (void)model;
    if (!m_captured) return true;
    return apply(m_previous);
}

bool UpdateTilesetCommand::apply(const State& state)
{
    auto* scene = ActiveScene();
    if (!scene) return false;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    dodoe::Tileset* tileset = FindTileset(tilemapEntity, m_tilesetAssetId);
    if (!tileset) return false;

    tileset->tile_width = state.tile_width;
    tileset->tile_height = state.tile_height;
    tileset->columns = state.columns;
    tileset->tile_count = state.tile_count;
    tileset->margin = state.margin;
    tileset->spacing = state.spacing;

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (auto* asset = assetManager
                          ? assetManager->loadAssetSync<dodoe::TilesetAsset>(m_tilesetAssetId)
                          : nullptr) {
        asset->updateGrid(state.tile_width, state.tile_height, state.columns, state.tile_count,
                          state.margin, state.spacing);
        const std::filesystem::path source(asset->getSourcePath().c_str());
        const std::filesystem::path absolute = source.is_absolute()
            ? source
            : assetManager->getAssetDir() / source;
        (void)asset->saveToSource(dodoe::String(absolute.generic_string().c_str()));
    }

    tilemapEntity.getComponent<dodoe::TilemapComponent>().dirty = true;
    return true;
}

std::string UpdateTilesetCommand::label() const
{
    return "Edit Tileset (" + std::to_string(static_cast<std::uint64_t>(m_tilesetAssetId)) + ")";
}

} // namespace cakery
