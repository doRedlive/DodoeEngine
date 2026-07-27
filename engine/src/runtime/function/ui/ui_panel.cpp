// do@Redlive

#include "ui_panel.h"
#include "ui_render_batch.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    void UIPanel::onCollectRenderData(UIRenderBatch& batch) {
        if (!isVisible()) return;

        Rect screen_rect = getScreenRect();

        if (m_bg_color.a > 0 || m_bg_texture) {
            UISceneInfo info;
            info.setPosition(screen_rect.pos);
            info.setSize(screen_rect.size);
            info.setUV({m_bg_uv_rect.pos.x, m_bg_uv_rect.pos.y},
                        {m_bg_uv_rect.pos.x + m_bg_uv_rect.size.x, m_bg_uv_rect.pos.y + m_bg_uv_rect.size.y});
            info.setColor(m_bg_color.to_rgba32());
            if (m_bg_texture) info.setTexture(PPtr<Texture2D>(m_bg_texture));
            info.setDepth(getDepth());
            info.setFlags(0);
            info.setClipRect({});

            batch.addQuad(info);
        }

        if (m_clip_children) {
            batch.pushClipRect(screen_rect);
        }

        UIElement::onCollectRenderData(batch);

        if (m_clip_children) {
            batch.popClipRect();
        }
    }

} // namespace dodoe
