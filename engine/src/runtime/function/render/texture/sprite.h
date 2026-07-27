// do@Redlive

#pragma once

#include "dopch.h"

#include "texture.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"

namespace dodoe {

    class Sprite : public Object {
        PPtr<Texture2D> m_texture{};
        Float m_uv_min_x{0.0f};
        Float m_uv_min_y{0.0f};
        Float m_uv_max_x{1.0f};
        Float m_uv_max_y{1.0f};

    protected:
        void onDestroy() override {}
        void trace(TraceVisitor& v) const override {}

    public:
        using Object::Object;

        [[nodiscard]] const char* getObjectTypeName() const override { return "Sprite"; }

        void setTexture(const PPtr<Texture2D>& texture) { m_texture = texture; }
        void setUVRect(Float min_x, Float min_y, Float max_x, Float max_y) {
            m_uv_min_x = min_x; m_uv_min_y = min_y;
            m_uv_max_x = max_x; m_uv_max_y = max_y;
        }

        [[nodiscard]] const PPtr<Texture2D>& getTexture() const { return m_texture; }
        [[nodiscard]] Float getUVMinX() const { return m_uv_min_x; }
        [[nodiscard]] Float getUVMinY() const { return m_uv_min_y; }
        [[nodiscard]] Float getUVMaxX() const { return m_uv_max_x; }
        [[nodiscard]] Float getUVMaxY() const { return m_uv_max_y; }
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
