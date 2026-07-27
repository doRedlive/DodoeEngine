// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/math/math.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/texture/texture.h"

namespace dodoe {

    struct Rect;

    class UISceneInfo {
    public:
        UISceneInfo() = default;

        void setPosition(const Vector2f& position) { m_position = position; }
        void setSize(const Vector2f& size) { m_size = size; }
        void setUV(const Vector2f& uv_min, const Vector2f& uv_max) { m_uv_min = uv_min; m_uv_max = uv_max; }
        void setColor(const UInt32 color) { m_color = color; }
        void setDepth(const Float depth) { m_depth = depth; }
        void setFlags(const UInt32 flags) { m_flags = flags; }
        void setClipRect(const Rect& clip) { m_clip_rect = clip; }
        void setTexture(const PPtr<Texture2D>& texture) { m_texture = texture; }

        [[nodiscard]] const Vector2f& getPosition() const { return m_position; }
        [[nodiscard]] const Vector2f& getSize() const { return m_size; }
        [[nodiscard]] const Vector2f& getUVMin() const { return m_uv_min; }
        [[nodiscard]] const Vector2f& getUVMax() const { return m_uv_max; }
        [[nodiscard]] UInt32 getColor() const { return m_color; }
        [[nodiscard]] Float getDepth() const { return m_depth; }
        [[nodiscard]] UInt32 getFlags() const { return m_flags; }
        [[nodiscard]] const Rect& getClipRect() const { return m_clip_rect; }
        [[nodiscard]] const PPtr<Texture2D>& getTexture() const { return m_texture; }

        [[nodiscard]] UIInstance toInstance() const {
            UIInstance inst{};
            inst.position = m_position;
            inst.size = m_size;
            inst.uv_min = m_uv_min;
            inst.uv_max = m_uv_max;
            inst.color = m_color;
            inst.atlas_index = 0;
            if (auto* tex = m_texture.get()) {
                inst.atlas_index = tex->getSlot();
            }
            inst.depth = m_depth;
            inst.flags = m_flags;
            inst.clip_rect = m_clip_rect;
            return inst;
        }

    private:
        Vector2f m_position{};
        Vector2f m_size{};
        Vector2f m_uv_min{};
        Vector2f m_uv_max{1.0f, 1.0f};
        UInt32 m_color{0xFFFFFFFF};
        Float m_depth{0.0f};
        UInt32 m_flags{0};
        Rect m_clip_rect{};
        PPtr<Texture2D> m_texture{};
    };

} // namespace dodoe
