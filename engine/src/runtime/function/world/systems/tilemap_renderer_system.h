// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/world/systems/system.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    class TilemapRendererSystem : public System {
    public:
        ~TilemapRendererSystem() override = default;

        void update(Registry& reg, const float dt) override {
            (void)dt;

            auto* texture_manager = Application::Self().context().render_system->getTextureManager();
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

                    auto& layer_tr = reg.get<TransformComponent>(layer_entity);
                    (void)layer_tr;

                    const Size_t tile_count = layer.tiles.size();
                    for (Size_t i = 0; i < tile_count; ++i) {
                        const UInt32 gid = layer.tiles[i];
                        if (gid == 0) {
                            continue;
                        }

                        const TilesetAsset* tileset = map.findTilesetByGid(gid);
                        if (!tileset) {
                            continue;
                        }
                        const UInt32 columns = tileset->columns;
                        if (columns == 0) {
                            continue;
                        }

                        const UInt32 local_gid = gid - tileset->first_gid;
                        const UInt32 col = local_gid % columns;
                        const UInt32 row = local_gid / columns;

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
                            texture = texture_manager->loadTexture(tileset->texture_id);
                        }
                        if (!texture && !tileset->image_path.empty()) {
                            Identifier path_id = string2hash(tileset->image_path);
                            texture = texture_manager->loadTexture(path_id, tileset->image_path);
                        }
                        if (!texture) {
                            texture = texture_manager->loadFallbackTexture();
                        }
                        if (!texture || texture->width <= 0 || texture->height <= 0) {
                            continue;
                        }

                        const float tex_w = static_cast<float>(texture->width);
                        const float tex_h = static_cast<float>(texture->height);

                        const Vector4f uv_rect(uv_x0 / tex_w, uv_y0 / tex_h, uv_x1 / tex_w, uv_y1 / tex_h);

                        Renderer2D::DrawSprite(
                            texture->id,
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
    };

} // dodoe

