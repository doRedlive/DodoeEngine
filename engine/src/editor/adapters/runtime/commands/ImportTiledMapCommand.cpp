// do@Redlive

#include "ImportTiledMapCommand.h"

#include "CreateTileLayerCommand.h"
#include "TilemapDocumentRefs.h"
#include "adapters/runtime/services/UuidResolve.h"
#include "core/document/EditorDocumentModel.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/asset/types/tiled_map_asset.h"
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

nlohmann::json* FindLayerValue(EditorDocumentModel& model, dodoe::UUID uuid) {
    EditorEntity* entity = model.findEntity(static_cast<std::uint64_t>(uuid));
    if (!entity) return nullptr;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "TileLayerComponent") {
            return &component.value;
        }
    }
    return nullptr;
}

} // namespace

ImportTiledMapCommand::ImportTiledMapCommand(dodoe::String name, dodoe::UUID tiledMapAssetId,
                                             nlohmann::json position)
    : m_name(std::move(name))
    , m_tiledMapAssetId(tiledMapAssetId)
    , m_position(std::move(position))
{
}

void ImportTiledMapCommand::execute(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (!scene) return;

    auto& resourceManager = dodoe::ResourceManager::Self();
    auto* assetManager = resourceManager.getAssetManager();
    if (!assetManager) return;

    dodoe::TiledMapAsset* tiledMap =
        assetManager->loadAssetSync<dodoe::TiledMapAsset>(m_tiledMapAssetId);
    if (!tiledMap) return;

    const dodoe::UInt32 mapW = tiledMap->getMapWidth();
    const dodoe::UInt32 mapH = tiledMap->getMapHeight();
    const dodoe::UInt32 tileW = tiledMap->getTileWidth();
    const dodoe::UInt32 tileH = tiledMap->getTileHeight();
    if (mapW == 0 || mapH == 0 || tileW == 0 || tileH == 0) return;

    const std::filesystem::path tmjAbs(tiledMap->getSourcePath().c_str());
    const std::filesystem::path assetDir(assetManager->getAssetDir().string());
    if (tmjAbs.empty() || assetDir.empty()) return;

    dodoe::Entity tilemapEntity = ResolveEntity(scene, m_createdUuid);
    if (!tilemapEntity.valid()) {
        dodoe::UUID uuid = m_createdUuid.isValid() ? m_createdUuid : dodoe::UUID::Generate();
        tilemapEntity = scene->createEntity(uuid, m_name);
        if (!tilemapEntity.valid()) return;
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

    if (!m_created) {
        m_tilesetAssetIds.clear();

        for (const auto& ts : tiledMap->getTilesets()) {
            if (ts.image_path.empty() || ts.tile_width == 0 || ts.tile_height == 0) continue;
            const std::filesystem::path imageAbs = tmjAbs.parent_path() / std::filesystem::path(ts.image_path.c_str());
            if (!std::filesystem::exists(imageAbs)) continue;

            const dodoe::ObjectID imageRef = assetManager->ensureImported(
                dodoe::String(imageAbs.generic_string().c_str()));
            if (!imageRef.isValid()) continue;
            auto* texture = resourceManager.loadObjectByPath<dodoe::Texture2D>(
                dodoe::FileID(dodoe::String(imageAbs.generic_string().c_str())));
            if (!texture || texture->getWidth() <= 0 || texture->getHeight() <= 0) continue;

            dodoe::UInt32 columns = ts.columns;
            dodoe::UInt32 tileCount = ts.tile_count;
            if (columns == 0) {
                columns = static_cast<dodoe::UInt32>(texture->getWidth()) / ts.tile_width;
            }
            if (columns == 0) continue;
            if (tileCount == 0) {
                tileCount = columns * (static_cast<dodoe::UInt32>(texture->getHeight()) / ts.tile_height);
            }

            std::error_code ec;
            const std::filesystem::path relImage = std::filesystem::relative(imageAbs, assetDir, ec);
            if (ec || relImage.empty() || relImage.string().starts_with("..")) continue;
            const std::string imageUrl = relImage.generic_string();

            const std::string tmjStem = tmjAbs.stem().string();
            const std::string imageStem = imageAbs.stem().string();
            const std::string tsName = ts.name.empty() ? imageStem : std::string(ts.name.c_str());
            const std::filesystem::path tsxPath =
                tmjAbs.parent_path() / (tmjStem + "_" + imageStem + ".tsx");

            nlohmann::json tsx;
            tsx["Name"] = tsName;
            tsx["FirstGid"] = ts.first_gid;
            tsx["TileWidth"] = ts.tile_width;
            tsx["TileHeight"] = ts.tile_height;
            tsx["Columns"] = columns;
            tsx["TileCount"] = tileCount;
            tsx["ImagePath"] = imageUrl;
            tsx["TextureId"] = 0;
            {
                std::ofstream file(tsxPath);
                if (!file.is_open()) continue;
                file << tsx.dump(4);
                file.flush();
            }

            const dodoe::ObjectID tilesetRef = assetManager->ensureTilesetImported(
                dodoe::String(tsxPath.generic_string().c_str()));
            if (!tilesetRef.isValid()) continue;

            dodoe::Tileset* tileset = resourceManager.loadObject<dodoe::Tileset>(tilesetRef.asset_id, 0);
            if (!tileset) continue;
            tileset->name = dodoe::String(tsName.c_str());
            tileset->first_gid = ts.first_gid;
            tileset->tile_width = ts.tile_width;
            tileset->tile_height = ts.tile_height;
            tileset->columns = columns;
            tileset->tile_count = tileCount;
            tileset->image_path = dodoe::String(imageUrl.c_str());

            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            tm.tilesets.push_back(dodoe::PPtr<dodoe::Tileset>(tileset));
            tm.dirty = true;
            m_tilesetAssetIds.push_back(tilesetRef.asset_id);

            if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_createdUuid)) {
                tilesets->push_back(nlohmann::json{
                    {"asset_id", static_cast<std::uint64_t>(tilesetRef.asset_id)},
                    {"sub_object_id", 0},
                });
            }
        }

        m_layerUuids.clear();
        for (const auto& layerData : tiledMap->getLayers()) {
            CreateTileLayerCommand layerCmd(m_createdUuid, layerData.name, layerData.width, layerData.height);
            layerCmd.execute(model);
            dodoe::UUID layerUuid = layerCmd.created();
            if (!layerUuid.isValid()) continue;
            m_layerUuids.push_back(layerUuid);

            dodoe::Entity layerEntity = ResolveEntity(scene, layerUuid);
            if (layerEntity.valid() && layerEntity.hasComponent<dodoe::TileLayerComponent>()) {
                auto& layer = layerEntity.getComponent<dodoe::TileLayerComponent>();
                layer.visible = layerData.visible;
                layer.opacity = layerData.opacity;
                layer.offset_x = layerData.offset_x;
                layer.offset_y = layerData.offset_y;
                const dodoe::UInt32 w = layerData.width;
                const dodoe::UInt32 h = layerData.height;
                const dodoe::Size_t count = layerData.tiles.size();
                for (dodoe::UInt32 ty = 0; ty < h; ++ty) {
                    const dodoe::UInt32 tiledRow = h - 1 - ty;
                    for (dodoe::UInt32 tx = 0; tx < w; ++tx) {
                        const dodoe::Size_t src = static_cast<dodoe::Size_t>(tiledRow) * w + tx;
                        const dodoe::UInt32 gid = src < count ? layerData.tiles[src] : 0;
                        layer.setTile(tx, ty, gid);
                    }
                }
            }

            if (nlohmann::json* value = FindLayerValue(model, layerUuid)) {
                nlohmann::json tiles = nlohmann::json::array();
                const dodoe::UInt32 w = layerData.width;
                const dodoe::UInt32 h = layerData.height;
                const dodoe::Size_t count = layerData.tiles.size();
                for (dodoe::UInt32 ty = 0; ty < h; ++ty) {
                    const dodoe::UInt32 tiledRow = h - 1 - ty;
                    for (dodoe::UInt32 tx = 0; tx < w; ++tx) {
                        const dodoe::Size_t src = static_cast<dodoe::Size_t>(tiledRow) * w + tx;
                        tiles.push_back(src < count ? layerData.tiles[src] : 0);
                    }
                }
                (*value)["tiles"] = std::move(tiles);
                (*value)["visible"] = layerData.visible;
                (*value)["opacity"] = layerData.opacity;
                (*value)["offset_x"] = layerData.offset_x;
                (*value)["offset_y"] = layerData.offset_y;
            }
        }
        m_created = true;
    }
}

void ImportTiledMapCommand::revert(EditorDocumentModel& model)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto tilemapEntity = ResolveEntity(scene, m_createdUuid);
        if (tilemapEntity.valid() && tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
            auto& tm = tilemapEntity.getComponent<dodoe::TilemapComponent>();
            auto& tilesets = tm.tilesets;
            tilesets.erase(
                std::remove_if(tilesets.begin(), tilesets.end(), [this](const dodoe::PPtr<dodoe::Tileset>& ref) {
                    return std::find(m_tilesetAssetIds.begin(), m_tilesetAssetIds.end(),
                                     ref.getObjectID().asset_id) != m_tilesetAssetIds.end();
                }),
                tilesets.end());
            tm.dirty = true;
        }

        for (const dodoe::UUID layerUuid : m_layerUuids) {
            auto layerEntity = ResolveEntity(scene, layerUuid);
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
        }

        if (tilemapEntity.valid()) {
            scene->destroyEntity(tilemapEntity);
        }
    }

    if (nlohmann::json* tilesets = FindTilemapTilesetsArray(model, m_createdUuid)) {
        tilesets->erase(
            std::remove_if(tilesets->begin(), tilesets->end(), [this](const nlohmann::json& item) {
                if (!item.contains("asset_id") || !item["asset_id"].is_number_unsigned()) {
                    return false;
                }
                const dodoe::UUID assetId(item["asset_id"].get<std::uint64_t>());
                return std::find(m_tilesetAssetIds.begin(), m_tilesetAssetIds.end(), assetId) !=
                       m_tilesetAssetIds.end();
            }),
            tilesets->end());
    }

    if (nlohmann::json* order = FindTilemapLayerOrderArray(model, m_createdUuid)) {
        order->erase(
            std::remove_if(order->begin(), order->end(), [this](const nlohmann::json& item) {
                if (!item.is_number_unsigned()) {
                    return false;
                }
                const dodoe::UUID layerUuid(item.get<std::uint64_t>());
                return std::find(m_layerUuids.begin(), m_layerUuids.end(), layerUuid) !=
                       m_layerUuids.end();
            }),
            order->end());
    }

    for (const dodoe::UUID layerUuid : m_layerUuids) {
        model.deleteEntity(static_cast<std::uint64_t>(layerUuid));
    }
    model.deleteEntity(static_cast<std::uint64_t>(m_createdUuid));

    m_created = false;
}

std::string ImportTiledMapCommand::label() const
{
    return std::string("Import TiledMap (") + m_name.c_str() + ")";
}

} // namespace cakery
