#pragma once

#include "dopch.h"
#include "ui_element.h"
#include "ui_defaults.h"
#include "runtime/core/utils/util.h"
#include "engine/render/text_renderer.h"

namespace dodoe {

class UILabel final : public UIElement {
private:
    engine::render::TextRenderer& text_renderer_;

    std::string text_;
    std::string font_path_;
    identifier font_id_;
    int font_size_;
    identifier style_id_{entt::null};
    using TextRenderOverrides = engine::utils::TextRenderOverrides;
    TextRenderOverrides overrides_{};
    std::uint64_t last_layout_revision_{0};

public:
    UILabel(engine::render::TextRenderer& text_renderer,
            std::string_view text,
            std::string_view font_path = DEFAULT_UI_FONT_PATH,
            int font_size = DEFAULT_UI_FONT_SIZE_PX,
            Vector2f position = {0.0f, 0.0f},
            std::optional<Color> text_color = std::nullopt);

    using UIElement::render;

    std::string_view getText() const { return text_; }
    identifier getFontId() const { return font_id_; }
    int getFontSize() const { return font_size_; }
    identifier getStyleId() const { return style_id_; }
    std::string_view getStyleKey() const {
        if (style_id_ == entt::null || !text_renderer_.hasTextStyle(style_id_)) {
            return text_renderer_.getDefaultUIStyleKey();
        }
        return text_renderer_.getTextStyleKey(style_id_);
    }

    void setText(std::string_view text);
    void setFontPath(std::string_view font_path);
    void setFontSize(int font_size);
    void setStyleKey(std::string_view style_key);
    void setTextColor(Color text_color);
    void setShadowColor(Color shadow_color);
    void setShadowOffset(Vector2f shadow_offset);
    void setShadowEnabled(bool enabled);
    void clearOverrides();
    
protected:
    void update(float delta_time, engine::core::Context& context) override;
    void renderSelf(engine::core::Context& context) override;

private:
    void refreshSize();
};


} // namespace dodoe
