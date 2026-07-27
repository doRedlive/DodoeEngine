// do@Redlive

#pragma once

#include "dopch.h"

#include "render_object.h"
#include "sprite_scene_info.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/texture/sprite.h"

namespace dodoe {

    class SpriteRenderObject : public RenderObject {
    private:
        PPtr<Sprite> m_sprite{};
        Float m_uv_min_x{0.0f};
        Float m_uv_min_y{0.0f};
        Float m_uv_max_x{1.0f};
        Float m_uv_max_y{1.0f};
        UInt32 m_atlas_index{0};
        UInt32 m_color{0xFFFFFFFF};
        UInt8 m_sorting_layer{0};
        UInt16 m_order_in_layer{0};
        UInt32 m_material_id{0};
        UInt32 m_flags{0};
        Bool m_visible{true};
        Bool m_cast_shadow{false};

    public:
        SpriteRenderObject() = default;

        void setSprite(const PPtr<Sprite>& sprite) {
            m_sprite = sprite;
            if (auto* sp = sprite.get()) {
                m_atlas_index = sp->getAtlasIndex();
                m_uv_min_x = sp->getUVMinX();
                m_uv_min_y = sp->getUVMinY();
                m_uv_max_x = sp->getUVMaxX();
                m_uv_max_y = sp->getUVMaxY();
            }
        }
        void setAtlasIndex(UInt32 index) { m_atlas_index = index; }
        void setUVRect(Float min_x, Float min_y, Float max_x, Float max_y) {
            m_uv_min_x = min_x; m_uv_min_y = min_y;
            m_uv_max_x = max_x; m_uv_max_y = max_y;
        }
        void setColor(UInt32 color) { m_color = color; }
        void setSortingLayer(UInt8 layer, UInt16 order) {
            m_sorting_layer = layer;
            m_order_in_layer = order;
        }
        void setMaterialId(UInt32 material_id) { m_material_id = material_id; }
        void setFlags(UInt32 flags) { m_flags = flags; }
        void setVisible(Bool visible) { m_visible = visible; }
        void setCastShadow(Bool cast_shadow) { m_cast_shadow = cast_shadow; }

        [[nodiscard]] const PPtr<Sprite>& getSprite() const { return m_sprite; }
        [[nodiscard]] UInt32 getAtlasIndex() const { return m_atlas_index; }
        [[nodiscard]] Float getUVMinX() const { return m_uv_min_x; }
        [[nodiscard]] Float getUVMinY() const { return m_uv_min_y; }
        [[nodiscard]] Float getUVMaxX() const { return m_uv_max_x; }
        [[nodiscard]] Float getUVMaxY() const { return m_uv_max_y; }
        [[nodiscard]] UInt32 getColor() const { return m_color; }
        [[nodiscard]] UInt32 getMaterialId() const { return m_material_id; }
        [[nodiscard]] UInt32 getFlags() const { return m_flags; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }

        [[nodiscard]] SpriteInstance buildSpriteInstance(const Vector2f& position, const Vector2f& scale, Float rotation, UInt32 sorting_key) const;

        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::Sprite; }
        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
    };

} // namespace dodoe
