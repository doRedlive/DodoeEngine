// do@Redlive

#include "tilemap_renderer_system.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/sprite_render_object.h"

namespace dodoe {

    TilemapRendererSystem::~TilemapRendererSystem() = default;

    void TilemapRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto tilemap_view = reg.view<IDComponent, TilemapComponent, TransformComponent, HierarchyComponent>();
        UnorderedSet<Uuid> active_tiles{};

        for (auto entity : tilemap_view) {
            auto& tm = entity.getComponent<TilemapComponent>();
            if (tm.dirty) {
                syncTilemap(entity);
            }

            auto& id = entity.getComponent<IDComponent>();
            auto& hier = entity.getComponent<HierarchyComponent>();

            Size_t layer_index = 0;
            for (auto& child : hier.children) {
                if (!child.valid() || !child.hasComponent<TileLayerComponent>()) {
                    layer_index++;
                    continue;
                }

                auto& layer = child.getComponent<TileLayerComponent>();
                if (!layer.visible) {
                    layer_index++;
                    continue;
                }

                for (Size_t i = 0; i < layer.tiles.size(); i++) {
                    UInt32 gid = layer.tiles[i];
                    if (gid == 0) continue;

                    Int32 tx = static_cast<Int32>(i % layer.layer_width);
                    Int32 ty = static_cast<Int32>(i / layer.layer_width);

                    TileKey key{id.id, layer_index, tx, ty};
                    Uuid tile_uuid = MakeTileUuid(id.id, layer_index, tx, ty);
                    active_tiles.insert(tile_uuid);

                    if (m_submitted_tiles.find(key) != m_submitted_tiles.end()) {
                        continue;
                    }

                    const TilesetAsset* tileset = tm.findTilesetByGid(gid);
                    if (!tileset || tileset->columns == 0) continue;

                    UInt32 local_gid = gid - tileset->first_gid;
                    UInt32 col = local_gid % tileset->columns;
                    UInt32 row = local_gid / tileset->columns;
                    UInt32 total_rows = (tileset->tile_count + tileset->columns - 1) / tileset->columns;

                    Float u0 = static_cast<Float>(col) / static_cast<Float>(tileset->columns);
                    Float v0 = static_cast<Float>(row) / static_cast<Float>(total_rows);
                    Float u1 = static_cast<Float>(col + 1) / static_cast<Float>(tileset->columns);
                    Float v1 = static_cast<Float>(row + 1) / static_cast<Float>(total_rows);

                    Float pos_x = static_cast<Float>(tx * tm.tile_width + layer.offset_x);
                    Float pos_y = static_cast<Float>(ty * tm.tile_height + layer.offset_y);
                    Float tile_w = static_cast<Float>(tm.tile_width);
                    Float tile_h = static_cast<Float>(tm.tile_height);

                    auto sprite = create_scope<SpriteRenderObject>();
                    sprite->setUUID(tile_uuid);
                    sprite->setUVRect(u0, v0, u1, v1);
                    sprite->setColor(0xFFFFFFFF);
                    sprite->setVisible(true);
                    sprite->setWorldTransform(BuildTileWorldMatrix(pos_x, pos_y, tile_w, tile_h));

                    FileID file_id(tileset->image_path);
                    sprite->setTexture(PPtr<Texture>(file_id, Uuid()));

                    RenderCommandQueue::AddSprite(std::move(sprite));
                    m_submitted_tiles[key] = tile_uuid;
                }
                layer_index++;
            }
        }

        pruneRemovedTiles(active_tiles);
    }

    void TilemapRendererSystem::syncTilemap(Entity entity) {
        auto& tm = entity.getComponent<TilemapComponent>();
        auto& id = entity.getComponent<IDComponent>();

        for (auto it = m_submitted_tiles.begin(); it != m_submitted_tiles.end();) {
            if (it->first.tilemap_uuid == id.id) {
                RenderCommandQueue::RemoveSprite(it->second);
                it = m_submitted_tiles.erase(it);
            } else {
                ++it;
            }
        }

        tm.dirty = false;
    }

    void TilemapRendererSystem::pruneRemovedTiles(const UnorderedSet<Uuid>& active_tiles) {
        for (auto it = m_submitted_tiles.begin(); it != m_submitted_tiles.end();) {
            if (active_tiles.find(it->second) == active_tiles.end()) {
                RenderCommandQueue::RemoveSprite(it->second);
                it = m_submitted_tiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    Uuid TilemapRendererSystem::MakeTileUuid(Uuid tilemap_uuid, Size_t layer_index, Int32 tx, Int32 ty) {
        uint64_t base = static_cast<uint64_t>(tilemap_uuid);
        uint64_t h = base ^ 0x9E3779B97F4A7C15ULL;
        h ^= static_cast<uint64_t>(layer_index) * 0x9E3779B97F4A7C15ULL;
        h ^= static_cast<uint64_t>(tx) * 2654435761ULL;
        h ^= static_cast<uint64_t>(ty) * 0x517CC1B727220A95ULL;
        h = h ^ (h >> 33);
        h = h * 0xFF51AFD7ED558CCDULL;
        h = h ^ (h >> 33);
        return Uuid(h);
    }

    Matrix4f TilemapRendererSystem::BuildTileWorldMatrix(Float pos_x, Float pos_y, Float width, Float height) {
        Matrix4f world(1.0f);
        world = Math::Translate(world, Vector3f(pos_x, pos_y, 0.0f));
        world = Math::Scale(world, Vector3f(width, height, 1.0f));
        return world;
    }

} // dodoe
