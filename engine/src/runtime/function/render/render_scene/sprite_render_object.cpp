// do@Redlive

#include "sprite_render_object.h"

namespace dodoe {

    SpriteInstance SpriteRenderObject::buildSpriteInstance(const Vector2f& position, const Vector2f& scale, Float rotation, UInt32 sorting_key) const {
        SpriteInstance instance{};
        instance.position_x = position.x;
        instance.position_y = position.y;
        instance.scale_x = scale.x;
        instance.scale_y = scale.y;
        instance.rotation = rotation;
        instance.atlas_index = m_atlas_index;
        instance.uv_min_x = m_uv_min_x;
        instance.uv_min_y = m_uv_min_y;
        instance.uv_max_x = m_uv_max_x;
        instance.uv_max_y = m_uv_max_y;
        instance.color = m_color;
        instance.sorting_key = sorting_key;
        instance.material_id = m_material_id;
        instance.flags = m_flags;
        return instance;
    }

    RenderObjectDirtyFlags SpriteRenderObject::diff(const RenderObject& previous) const {
        if (previous.getRenderObjectType() != RenderObjectType::Sprite) {
            return RenderObjectDirtyFlags::All;
        }
        const auto& prev = static_cast<const SpriteRenderObject&>(previous);
        RenderObjectDirtyFlags flags = RenderObjectDirtyFlags::None;
        if (m_uv_min_x != prev.m_uv_min_x ||
            m_uv_min_y != prev.m_uv_min_y ||
            m_uv_max_x != prev.m_uv_max_x ||
            m_uv_max_y != prev.m_uv_max_y) {
            flags |= RenderObjectDirtyFlags::Mesh;
        }
        if (m_material_id != prev.m_material_id) {
            flags |= RenderObjectDirtyFlags::Materials;
        }
        if (m_color != prev.m_color ||
            m_flags != prev.m_flags ||
            m_visible != prev.m_visible ||
            m_cast_shadow != prev.m_cast_shadow) {
            flags |= RenderObjectDirtyFlags::State;
        }
        return flags;
    }

} // namespace dodoe
