// do@Redlive

#include "tilemap_renderer_system.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/sprite_render_object.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    namespace {
        constexpr UInt32 kChunkSize = 32;

        UInt32 MakeLayerSortingKey(const Size_t layer_index) {
            return static_cast<UInt32>(layer_index) + 1;
        }
    }

    TilemapRendererSystem::~TilemapRendererSystem() = default;

    SystemAccess TilemapRendererSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TilemapComponent, TransformComponent, HierarchyComponent, TileLayerComponent>()
            .build();
    }

    void TilemapRendererSystem::update(Registry& reg, float dt) {
        (void)dt;
        auto tilemap_view = reg.view<IDComponent, TilemapComponent, TransformComponent, HierarchyComponent>();
        UnorderedSet<UUID> active_chunks{};

        for (auto entity : tilemap_view) {
            auto& tm = entity.getComponent<TilemapComponent>();
            auto& id = entity.getComponent<IDComponent>();
            auto [submitted_it, inserted] = m_submitted_chunks.try_emplace(id.id);
            auto& submitted = submitted_it->second;
            const UInt64 signature = ComputeTilemapSignature(entity);
            const auto signature_it = m_tilemap_signatures.find(id.id);
            if (tm.dirty || inserted || signature_it == m_tilemap_signatures.end() ||
                signature_it->second != signature) {
                syncTilemap(entity);
            }
            m_tilemap_signatures[id.id] = signature;
            for (const UUID chunk_id : submitted) {
                active_chunks.insert(chunk_id);
            }
        }
        pruneRemovedChunks(active_chunks);
    }

    void TilemapRendererSystem::syncTilemap(Entity entity) {
        auto& tm = entity.getComponent<TilemapComponent>();
        auto& id = entity.getComponent<IDComponent>();
        auto& hier = entity.getComponent<HierarchyComponent>();
        auto& submitted = m_submitted_chunks[id.id];

        for (const UUID chunk_id : submitted) {
            RenderCommandQueue::RemoveSprite(chunk_id);
        }
        submitted.clear();

        UnorderedMap<const Tileset*, UInt32> atlas_indices{};
        atlas_indices.reserve(tm.tilesets.size());
        for (auto& tileset_ref : tm.tilesets) {
            if (!tileset_ref.get() && tileset_ref.getObjectID().isValid()) {
                const ObjectID& ref = tileset_ref.getObjectID();
                if (Tileset* resolved = ResourceManager::Self().loadObject<Tileset>(ref.asset_id, ref.local_id)) {
                    tileset_ref = PPtr<Tileset>(resolved);
                }
            }
            const auto* tileset = tileset_ref.get();
            if (!tileset) continue;
            UInt32 atlas_index = 0;
            if (auto* tex = ResourceManager::Self().loadObjectByPath<Texture2D>(FileID(tileset->image_path))) {
                atlas_index = tex->getDescriptorIndex() >= 0
                    ? static_cast<UInt32>(tex->getDescriptorIndex())
                    : tex->getSlot();
            }
            atlas_indices.emplace(tileset, atlas_index);
        }

        Size_t layer_index = 0;
        for (auto& child : hier.children) {
            if (!child.valid() || !child.hasComponent<TileLayerComponent>()) {
                ++layer_index;
                continue;
            }
            const auto& layer = child.getComponent<TileLayerComponent>();
            if (!layer.visible || layer.layer_width == 0 || layer.layer_height == 0) {
                ++layer_index;
                continue;
            }

            const UInt32 chunk_cols = (layer.layer_width + kChunkSize - 1) / kChunkSize;
            const UInt32 chunk_rows = (layer.layer_height + kChunkSize - 1) / kChunkSize;
            for (UInt32 chunk_y = 0; chunk_y < chunk_rows; ++chunk_y) {
                for (UInt32 chunk_x = 0; chunk_x < chunk_cols; ++chunk_x) {
                    const UInt32 x0 = chunk_x * kChunkSize;
                    const UInt32 y0 = chunk_y * kChunkSize;
                    const UInt32 x1 = std::min(layer.layer_width, x0 + kChunkSize);
                    const UInt32 y1 = std::min(layer.layer_height, y0 + kChunkSize);
                    const Float min_x = static_cast<Float>(x0 * tm.tile_width + layer.offset_x);
                    const Float min_y = static_cast<Float>(y0 * tm.tile_height + layer.offset_y);
                    const Float max_x = static_cast<Float>(x1 * tm.tile_width + layer.offset_x);
                    const Float max_y = static_cast<Float>(y1 * tm.tile_height + layer.offset_y);

                    DynamicArray<SpriteInstance> instances{};
                    instances.reserve(static_cast<Size_t>(x1 - x0) * (y1 - y0));
                    for (UInt32 ty = y0; ty < y1; ++ty) {
                        for (UInt32 tx = x0; tx < x1; ++tx) {
                            const Size_t index = static_cast<Size_t>(ty) * layer.layer_width + tx;
                            if (index >= layer.tiles.size()) continue;
                            const UInt32 gid = layer.tiles[index];
                            if (gid == 0) continue;
                            const Tileset* tileset = tm.findTilesetByGid(gid);
                            if (!tileset || tileset->columns == 0 || tileset->tile_count == 0) continue;

                            const UInt32 local_gid = gid - tileset->first_gid;
                            const UInt32 col = local_gid % tileset->columns;
                            const UInt32 row = local_gid / tileset->columns;
                            const UInt32 total_rows = (tileset->tile_count + tileset->columns - 1) / tileset->columns;
                            if (row >= total_rows) continue;

                            const auto atlas_it = atlas_indices.find(tileset);
                            if (atlas_it == atlas_indices.end()) continue;

                            SpriteInstance instance{};
                            instance.position_x = static_cast<Float>(tx * tm.tile_width + layer.offset_x);
                            instance.position_y = static_cast<Float>(ty * tm.tile_height + layer.offset_y);
                            instance.scale_x = static_cast<Float>(tm.tile_width);
                            instance.scale_y = static_cast<Float>(tm.tile_height);
                            instance.atlas_index = atlas_it->second;
                            instance.uv_min_x = static_cast<Float>(col) / tileset->columns;
                            instance.uv_min_y = static_cast<Float>(row) / total_rows;
                            instance.uv_max_x = static_cast<Float>(col + 1) / tileset->columns;
                            instance.uv_max_y = static_cast<Float>(row + 1) / total_rows;
                            instance.color = 0xFFFFFFFF;
                            instance.sorting_key = MakeLayerSortingKey(layer_index);
                            instances.push_back(instance);
                        }
                    }

                    if (!instances.empty()) {
                        auto chunk = create_scope<SpriteRenderObject>();
                        chunk->setUUID(MakeChunkUuid(id.id, layer_index, chunk_x, chunk_y));
                        chunk->setVisible(true);
                        chunk->setSortingKey(MakeLayerSortingKey(layer_index));
                        chunk->setBatchInstances(std::move(instances));
                        chunk->setBounds(
                            Vector3f((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f, 0.0f),
                            Vector3f((max_x - min_x) * 0.5f, (max_y - min_y) * 0.5f, 0.01f));
                        submitted.push_back(chunk->getUUID());
                        RenderCommandQueue::AddSprite(std::move(chunk));
                    }
                }
            }
            ++layer_index;
        }
        tm.dirty = false;
    }

    void TilemapRendererSystem::pruneRemovedChunks(const UnorderedSet<UUID>& active_chunks) {
        for (auto it = m_submitted_chunks.begin(); it != m_submitted_chunks.end();) {
            auto& chunks = it->second;
            for (auto chunk_it = chunks.begin(); chunk_it != chunks.end();) {
                if (active_chunks.find(*chunk_it) == active_chunks.end()) {
                    RenderCommandQueue::RemoveSprite(*chunk_it);
                    chunk_it = chunks.erase(chunk_it);
                } else {
                    ++chunk_it;
                }
            }
            if (chunks.empty()) {
                m_tilemap_signatures.erase(it->first);
                it = m_submitted_chunks.erase(it);
            } else {
                ++it;
            }
        }
    }

    UUID TilemapRendererSystem::MakeChunkUuid(UUID tilemap_uuid, Size_t layer_index,
                                               UInt32 chunk_x, UInt32 chunk_y) {
        uint64_t h = static_cast<uint64_t>(tilemap_uuid) ^ 0x9E3779B97F4A7C15ULL;
        h ^= static_cast<uint64_t>(layer_index) * 0x517CC1B727220A95ULL;
        h ^= static_cast<uint64_t>(chunk_x) * 2654435761ULL;
        h ^= static_cast<uint64_t>(chunk_y) * 0xFF51AFD7ED558CCDULL;
        h ^= h >> 33;
        h *= 0xC4CEB9FE1A85EC53ULL;
        h ^= h >> 33;
        return UUID(h);
    }

    UInt64 TilemapRendererSystem::ComputeTilemapSignature(Entity entity) {
        if (!entity.valid() || !entity.hasComponent<HierarchyComponent>()) return 0;
        const auto& tm = entity.getComponent<TilemapComponent>();
        const auto& hier = entity.getComponent<HierarchyComponent>();
        UInt64 h = 1469598103934665603ULL;
        auto mix = [&h](UInt64 value) {
            h ^= value;
            h *= 1099511628211ULL;
        };
        mix(static_cast<UInt64>(tm.tile_width));
        mix(static_cast<UInt64>(tm.tile_height));
        for (auto& child : hier.children) {
            if (!child.valid() || !child.hasComponent<TileLayerComponent>()) {
                mix(0);
                continue;
            }
            const auto& layer = child.getComponent<TileLayerComponent>();
            mix(layer.layer_width);
            mix(layer.layer_height);
            mix(layer.visible ? 1 : 0);
            mix(static_cast<UInt64>(static_cast<Int64>(layer.offset_x)));
            mix(static_cast<UInt64>(static_cast<Int64>(layer.offset_y)));
            mix(static_cast<UInt64>(layer.tiles.size()));
        }
        return h;
    }

} // namespace dodoe
