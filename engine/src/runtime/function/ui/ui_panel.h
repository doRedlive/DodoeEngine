// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"
#include "ui_types.h"

namespace dodoe {

    class Texture2D;

    class UIPanel : public UIElement {
    private:
        Color m_bg_color{0, 0, 0, 0};
        Texture2D* m_bg_texture{nullptr};
        Rect m_bg_uv_rect{0, 0, 1, 1};
        NineSliceMargins m_nine_slice{};
        Bool m_use_nine_slice{false};
        Bool m_clip_children{false};

    public:
        void setBackgroundColor(Color color) { m_bg_color = color; }
        [[nodiscard]] Color getBackgroundColor() const { return m_bg_color; }
        void setBackgroundImage(Texture2D* texture, Rect uv = {0, 0, 1, 1}) { m_bg_texture = texture; m_bg_uv_rect = uv; }
        void setNineSlice(NineSliceMargins margins) { m_nine_slice = margins; }
        void setClipChildren(Bool clip) { m_clip_children = clip; }
        [[nodiscard]] Bool isClipChildrenEnabled() const { return m_clip_children; }

    protected:
        void onCollectRenderData(class UIRenderBatch& batch) override;
    };

} // namespace dodoe
