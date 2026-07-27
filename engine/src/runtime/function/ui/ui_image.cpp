// do@Redlive

#include "ui_image.h"
#include "ui_render_batch.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/core/utils/util.h"
#include <algorithm>

namespace dodoe {

    void UIImage::onLayout() {
        if (!m_preserve_aspect) return;

        Float uv_w = m_uv_rect.size.x;
        Float uv_h = m_uv_rect.size.y;
        if (uv_w <= 0 || uv_h <= 0) return;

        Float aspect = uv_w / uv_h;
        Float current_aspect = m_size.x / m_size.y;

        Vector2f adjusted = m_size;
        if (current_aspect > aspect) {
            adjusted.x = m_size.y * aspect;
        } else {
            adjusted.y = m_size.x / aspect;
        }

        if (m_parent) {
            Vector2f parent_size = m_parent->getLayoutSize();
            Vector2f anchor_min = getAnchorMin();
            Vector2f anchor_max = getAnchorMax();

            if (!(anchor_min.x == anchor_max.x)) adjusted.x = parent_size.x * (anchor_max.x - anchor_min.x) * aspect;
        }

        if (adjusted.x != m_size.x || adjusted.y != m_size.y) {
            m_size = adjusted;
        }
    }

    void UIImage::onCollectRenderData(UIRenderBatch& batch) {
        if (!isVisible()) return;

        Rect screen_rect = getScreenRect();

        UISceneInfo info;
        info.setPosition(screen_rect.pos);
        info.setSize(screen_rect.size);

        Float u0 = m_uv_rect.pos.x;
        Float v0 = m_uv_rect.pos.y;
        Float u1 = m_uv_rect.pos.x + m_uv_rect.size.x;
        Float v1 = m_uv_rect.pos.y + m_uv_rect.size.y;

        if (m_flip_h) std::swap(u0, u1);
        if (m_flip_v) std::swap(v0, v1);

        switch (m_fill_method) {
        case FillMethod::Horizontal:
            u1 = u0 + (u1 - u0) * m_fill_amount;
            break;
        case FillMethod::Vertical:
            v1 = v0 + (v1 - v0) * m_fill_amount;
            break;
        default:
            break;
        }

        info.setUV({u0, v0}, {u1, v1});
        info.setColor(m_color.to_rgba32());
        if (m_texture) info.setTexture(PPtr<Texture2D>(m_texture));
        info.setDepth(getDepth());
        info.setFlags(0);
        info.setClipRect({});

        batch.addQuad(info);

        UIElement::onCollectRenderData(batch);
    }

} // namespace dodoe
