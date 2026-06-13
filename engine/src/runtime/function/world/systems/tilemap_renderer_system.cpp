#include "tilemap_renderer_system.h"

#include "render_system_bridge.h"

namespace dodoe {

    TilemapRendererSystem::~TilemapRendererSystem() = default;

    void TilemapRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto* texture_manager = TryGetTextureManager();
        if (!texture_manager) {
            return;
        }

        auto map_view = reg.view<TilemapComponent, TransformComponent>();
        for (auto map_entity : map_view) {
            auto& map = reg.get<TilemapComponent>(map_entity);
            auto& map_tr = reg.get<TransformComponent>(map_entity);

            auto child_view = reg.view<TileLayerComponent, TransformComponent, HierarchyComponent>();
            for (auto layer_entity : child_view) {
                auto& layer = reg.get<TileLayerComponent>(layer_entity);
                if (!layer.visible) {
                    continue;
                }

                const Size_t tile_count = layer.tiles.size();
                for (Size_t i = 0; i < tile_count; ++i) {
                    const UInt32 gid = layer.tiles[i];
                    if (gid == 0) {
                        continue;
                    }

                    const TilesetAsset* tileset = map.findTilesetByGid(gid);
                    if (!tileset || tileset->columns == 0) {
                        continue;
                    }

                    const UInt32 local_gid = gid - tileset->first_gid;
                    const UInt32 col = local_gid % tileset->columns;
                    const UInt32 row = local_gid / tileset->columns;

                    const float tile_w = static_cast<float>(map.tile_width);
                    const float tile_h = static_cast<float>(map.tile_height);
                    const UInt32 layer_x = static_cast<UInt32>(i % layer.layer_width);
                    const UInt32 layer_y = static_cast<UInt32>(i / layer.layer_width);

                    const float pos_x = map_tr.position.x
                        + static_cast<float>(layer_x) * tile_w
                        + static_cast<float>(layer.offset_x);
                    const float pos_y = map_tr.position.y
                        + static_cast<float>(layer_y) * tile_h
                        + static_cast<float>(layer.offset_y);

                    const float ts_w = static_cast<float>(tileset->tile_width);
                    const float ts_h = static_cast<float>(tileset->tile_height);
                    const float uv_x0 = static_cast<float>(col) * ts_w;
                    const float uv_y0 = static_cast<float>(row) * ts_h;
                    const float uv_x1 = uv_x0 + ts_w;
                    const float uv_y1 = uv_y0 + ts_h;

                    Ref<Texture> texture = nullptr;
                    if (tileset->texture_id != 0) {
                        texture = texture_manager->findTexture(static_cast<InstanceID>(tileset->texture_id));
                    }
                    if (!texture && !tileset->image_path.empty()) {
                        texture = Texture::Load(tileset->image_path);
                    }
                    if (!texture) {
                        texture = texture_manager->getFallback();
                    }
                    if (!texture || texture->getWidth() <= 0 || texture->getHeight() <= 0) {
                        continue;
                    }

                    const float tex_w = static_cast<float>(texture->getWidth());
                    const float tex_h = static_cast<float>(texture->getHeight());
                    const Vector4f uv_rect(uv_x0 / tex_w, uv_y0 / tex_h, uv_x1 / tex_w, uv_y1 / tex_h);

                    Renderer2D::DrawSprite(
                        texture->getInstanceID(),
                        Vector2f(pos_x, pos_y),
                        Vector2f(tile_w, tile_h),
                        Vector3f(0.0f),
                        uv_rect,
                        Color::white()
                    );
                }
            }
        }
    }

} // dodoe
