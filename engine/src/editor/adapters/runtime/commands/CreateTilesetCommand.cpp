// do@Redlive

#include "CreateTilesetCommand.h"

#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

} // namespace

CreateTilesetCommand::CreateTilesetCommand(dodoe::UUID tilemap, dodoe::String imagePath,
                                           dodoe::UInt32 tileWidth, dodoe::UInt32 tileHeight,
                                           dodoe::UInt32 margin, dodoe::UInt32 spacing)
    : m_tilemap(tilemap)
    , m_imagePath(std::move(imagePath))
    , m_tileWidth(tileWidth)
    , m_tileHeight(tileHeight)
    , m_margin(margin)
    , m_spacing(spacing)
{}

void CreateTilesetCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return;
    auto tilemapEntity = ResolveEntity(scene, m_tilemap);
    if (!tilemapEntity.valid() || !tilemapEntity.hasComponent<dodoe::TilemapComponent>()) return;

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) return;

    const std::filesystem::path imageAbs(m_imagePath.c_str());
    const std::filesystem::path assetDir(assetManager->getAssetDir().string());
    if (!std::filesystem::exists(imageAbs) || assetDir.empty()) return;

    const dodoe::ObjectID imageRef = assetManager->ensureImported(m_imagePath);
    if (!imageRef.isValid()) return;
    auto* texture = resourceManager.loadObjectByPath<dodoe::Texture2D>(dodoe::FileID(m_imagePath));
    if (!texture || texture->getWidth() <= 0 || texture->getHeight() <= 0) return;

    const dodoe::Int32 imageW = texture->getWidth();
    const dodoe::Int32 imageH = texture->getHeight();
    if (m_tileWidth == 0 || m_tileHeight == 0) return;

    dodoe::UInt32 columns = 0;
    dodoe::UInt32 rows = 0;
    const dodoe::UInt32 stepW = m_tileWidth + m_spacing;
    const dodoe::UInt32 stepH = m_tileHeight + m_spacing;
    if (stepW > 0) {
        columns = static_cast<dodoe::UInt32>(
            (static_cast<dodoe::Int32>(imageW) - static_cast<dodoe::Int32>(m_margin * 2) + static_cast<dodoe::Int32>(m_spacing)) / static_cast<dodoe::Int32>(stepW));
    }
    if (stepH > 0) {
        rows = static_cast<dodoe::UInt32>(
            (static_cast<dodoe::Int32>(imageH) - static_cast<dodoe::Int32>(m_margin * 2) + static_cast<dodoe::Int32>(m_spacing)) / static_cast<dodoe::Int32>(stepH));
    }
    if (columns == 0 || rows == 0) return;
    const dodoe::UInt32 tileCount = columns * rows;

    dodoe::UInt32 firstGid = 1;
    auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
    for (auto& ref : tm.tilesets) {
        const dodoe::Tileset* tileset = ref.get();
        if (!tileset) continue;
        const dodoe::UInt32 end = tileset->first_gid + std::max(tileset->tile_count, 1u);
        if (end > firstGid) firstGid = end;
    }

    std::error_code ec;
    const std::filesystem::path relImage = std::filesystem::relative(imageAbs, assetDir, ec);
    if (ec || relImage.empty() || relImage.string().starts_with("..")) return;
    const std::string imageUrl = relImage.generic_string();

    const std::string baseName = imageAbs.stem().string();
    const std::filesystem::path tsxPath = imageAbs.parent_path() / (baseName + ".tsx");

    nlohmann::json tsx;
    tsx["Name"] = baseName;
    tsx["FirstGid"] = firstGid;
    tsx["TileWidth"] = m_tileWidth;
    tsx["TileHeight"] = m_tileHeight;
    tsx["Columns"] = columns;
    tsx["TileCount"] = tileCount;
    tsx["ImagePath"] = imageUrl;
    tsx["TextureId"] = 0;
    {
        std::ofstream file(tsxPath);
        if (!file.is_open()) return;
        file << tsx.dump(4);
        file.flush();
    }

    const dodoe::ObjectID tilesetRef = assetManager->ensureTilesetImported(
        dodoe::String(tsxPath.generic_string().c_str()));
    if (!tilesetRef.isValid()) return;
    m_createdAssetId = tilesetRef.asset_id;

    dodoe::Tileset* tileset = resourceManager.loadObject<dodoe::Tileset>(m_createdAssetId, 0);
    if (!tileset) return;
    tileset->name = dodoe::String(baseName.c_str());
    tileset->first_gid = firstGid;
    tileset->tile_width = m_tileWidth;
    tileset->tile_height = m_tileHeight;
    tileset->columns = columns;
    tileset->tile_count = tileCount;
    tileset->image_path = dodoe::String(imageUrl.c_str());

    tm.tilesets.push_back(dodoe::PPtr<dodoe::Tileset>(tileset));
    tm.dirty = true;

    if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_tilemap)) {
        tilesets->push_back(nlohmann::json{
            {"asset_id", static_cast<std::uint64_t>(m_createdAssetId)},
            {"sub_object_id", 0},
        });
    }
    m_created = true;
}

void CreateTilesetCommand::revert(EditorDocumentModel& model)
{
    if (!m_created) return;

    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_tilemap);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            auto& tilesets = tm.tilesets;
            tilesets.erase(
                std::remove_if(tilesets.begin(), tilesets.end(), [this](const dodoe::PPtr<dodoe::Tileset>& ref) {
                    return ref.getObjectID().asset_id == m_createdAssetId;
                }),
                tilesets.end());
            tm.dirty = true;
        }
    }

    if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_tilemap)) {
        tilesets->erase(
            std::remove_if(tilesets->begin(), tilesets->end(), [this](const nlohmann::json& item) {
                return item.contains("asset_id") &&
                       item["asset_id"].get<std::uint64_t>() == static_cast<std::uint64_t>(m_createdAssetId);
            }),
            tilesets->end());
    }
}

std::string CreateTilesetCommand::label() const
{
    return std::string("Add Tileset (") + m_imagePath.c_str() + ")";
}

} // namespace cakery
