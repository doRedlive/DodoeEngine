// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_widget.h"

namespace dodoe {

    class Texture2D;

    class UIImage : public UIWidget {
    private:
        Texture2D* m_texture{nullptr};
        Rect m_uv_rect{0, 0, 1, 1};
        Bool m_flip_h{false};
        Bool m_flip_v{false};
        FillMethod m_fill_method{FillMethod::None};
        Float m_fill_amount{1};
        Bool m_preserve_aspect{false};

    public:
        void setTexture(Texture2D* texture) { m_texture = texture; }
        void setUVRect(const Rect& uv) { m_uv_rect = uv; }
        void setFlipped(Bool horizontal, Bool vertical) { m_flip_h = horizontal; m_flip_v = vertical; }
        [[nodiscard]] Bool isFlippedH() const { return m_flip_h; }
        [[nodiscard]] Bool isFlippedV() const { return m_flip_v; }
        void setFlippedH(Bool v) { m_flip_h = v; }
        void setFlippedV(Bool v) { m_flip_v = v; }
        void setFillMethod(FillMethod method, Float fillAmount) { m_fill_method = method; m_fill_amount = fillAmount; }
        void setPreserveAspect(Bool preserve) { m_preserve_aspect = preserve; }
        [[nodiscard]] Bool isPreserveAspect() const { return m_preserve_aspect; }

    protected:
        void onCollectRenderData(class UIRenderBatch& batch) override;
        void onLayout() override;
    };

} // namespace dodoe
