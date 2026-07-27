// do@Redlive

#include "ui_label.h"
#include "ui_render_batch.h"
#include "runtime/function/render/texture/texture.h"
#include "runtime/core/utils/util.h"
#include <algorithm>
#include <sstream>

namespace dodoe {

    void UILabel::onLayout() {
        if (m_text.empty()) return;

        const Vector2f size = getSize();
        const Bool has_set_size = !(size.x == 0 && size.y == 0);
        if (has_set_size) return;

        const Float char_width = static_cast<Float>(m_font_size) * 0.6f;
        const Float line_height = static_cast<Float>(m_font_size) * m_line_spacing;

        std::istringstream stream(m_text);
        String line;
        Int line_count = 0;
        Float max_line_width = 0;

        while (std::getline(stream, line, '\n')) {
            ++line_count;
            Float line_w = static_cast<Float>(line.size()) * char_width;
            max_line_width = std::max(max_line_width, line_w);
        }

        if (line_count == 0) line_count = 1;

        setSize({max_line_width, static_cast<Float>(line_count) * line_height});
    }

    void UILabel::onCollectRenderData(UIRenderBatch& batch) {
        if (!isVisible() || m_text.empty()) return;

        Rect screen_rect = getScreenRect();

        UISceneInfo info;
        info.setPosition(screen_rect.pos);
        info.setSize(screen_rect.size);
        info.setUV({0, 0}, {1, 1});
        info.setColor(getColor().to_rgba32());
        if (m_font_atlas) info.setTexture(PPtr<Texture2D>(m_font_atlas));
        info.setDepth(getDepth());
        info.setFlags(0);
        info.setClipRect({});

        batch.addQuad(info);

        UIElement::onCollectRenderData(batch);
    }

} // namespace dodoe
