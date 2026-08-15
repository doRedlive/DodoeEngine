// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/texture/texture.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"

namespace dodoe {

    inline constexpr Float kDefaultPixelsPerUnit = 10.0f;

    class Sprite : public Object {
        PPtr<Texture2D> m_texture{};
        Float m_uv_min_x{0.0f};
        Float m_uv_min_y{0.0f};
        Float m_uv_max_x{1.0f};
        Float m_uv_max_y{1.0f};
        Float m_pixels_per_unit{kDefaultPixelsPerUnit};

    public:
        Sprite() = default;
        explicit Sprite(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Sprite"; }

        void setTexture(const PPtr<Texture2D>& texture) { m_texture = texture; }
        void setUVRect(Float min_x, Float min_y, Float max_x, Float max_y) {
            m_uv_min_x = min_x; m_uv_min_y = min_y;
            m_uv_max_x = max_x; m_uv_max_y = max_y;
        }
        void setPixelsPerUnit(Float pixels_per_unit) { m_pixels_per_unit = pixels_per_unit; }

        [[nodiscard]] const PPtr<Texture2D>& getTexture() const { return m_texture; }
        [[nodiscard]] Float getUVMinX() const { return m_uv_min_x; }
        [[nodiscard]] Float getUVMinY() const { return m_uv_min_y; }
        [[nodiscard]] Float getUVMaxX() const { return m_uv_max_x; }
        [[nodiscard]] Float getUVMaxY() const { return m_uv_max_y; }
        [[nodiscard]] Float getPixelsPerUnit() const { return m_pixels_per_unit; }
        [[nodiscard]] UInt32 getAtlasIndex() const {
            if (auto* tex = m_texture.get()) {
                if (tex->getDescriptorIndex() >= 0) {
                    return static_cast<UInt32>(tex->getDescriptorIndex());
                }
                return tex->getSlot();
            }
            return 0;
        }
    };

} // namespace dodoe
