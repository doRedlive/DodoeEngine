// do@Redlive

#include "sprite_scene_info.h"

namespace dodoe {

    SpriteSceneInfo::SpriteSceneInfo(const Identifier id)
        : m_id(id) {}

    void SpriteSceneInfo::setSprite(const PPtr<Sprite>& sprite) {
        m_sprite = sprite;
        if (auto* sp = sprite.get()) {
            m_atlas_index = sp->getAtlasIndex();
            m_uv_min_x = sp->getUVMinX();
            m_uv_min_y = sp->getUVMinY();
            m_uv_max_x = sp->getUVMaxX();
            m_uv_max_y = sp->getUVMaxY();
        }
    }

    void SpriteSceneInfo::setUVRect(const Float min_x, const Float min_y, const Float max_x, const Float max_y) {
        m_uv_min_x = min_x;
        m_uv_min_y = min_y;
        m_uv_max_x = max_x;
        m_uv_max_y = max_y;
    }

    SpriteInstance SpriteSceneInfo::toInstance() const {
        SpriteInstance instance{};
        instance.position_x = m_position.x;
        instance.position_y = m_position.y;
        instance.scale_x = m_scale.x;
        instance.scale_y = m_scale.y;
        instance.rotation = m_rotation;
        instance.atlas_index = m_atlas_index;
        instance.uv_min_x = m_uv_min_x;
        instance.uv_min_y = m_uv_min_y;
        instance.uv_max_x = m_uv_max_x;
        instance.uv_max_y = m_uv_max_y;
        instance.color = m_color;
        instance.sorting_key = m_sorting_key;
        instance.material_id = m_material_id;
        instance.flags = m_flags;
        return instance;
    }

} // namespace dodoe
