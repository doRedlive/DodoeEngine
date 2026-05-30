#include "ui_label.h"
#include "ui_compat.h"
#include <entt/core/hashed_string.hpp>

namespace dodoe {

namespace {

using LayoutOptions = LayoutOptions;
using TextRenderOverrides = TextRenderOverrides;

[[nodiscard]] LayoutOptions resolveLabelLayout(const TextRenderer& renderer,
                                              identifier style_id,
                                              const TextRenderOverrides& overrides) {
    const identifier resolved_style =
        (style_id == entt::null || !renderer.hasTextStyle(style_id)) ? renderer.getDefaultUIStyleId() : style_id;
    auto layout = renderer.getTextStyle(resolved_style).layout;
    if (overrides.glyph_scale) {
        layout.glyph_scale = *overrides.glyph_scale;
    }
    return layout;
}

} // namespace

UILabel::UILabel(TextRenderer& text_renderer,
                 std::string_view text,
                 std::string_view font_path,
                 int font_size,
                 Vector2f position,
                 std::optional<Color> text_color
                )
    : UIElement(std::move(position)),
      text_renderer_(text_renderer),
      text_(text),
      font_path_(font_path),
      font_id_(entt::hashed_string{font_path.data(), font_path.size()}),
      font_size_(font_size) {
    if (text_color) {
        overrides_.color = *text_color;
    }
    last_layout_revision_ = text_renderer_.getLayoutRevision();
    refreshSize();
    DO_TRACE("UILabel created.");
}

void UILabel::update(float delta_time, Context& context) {
    const auto revision = text_renderer_.getLayoutRevision();
    if (revision != last_layout_revision_) {
        refreshSize();
        last_layout_revision_ = revision;
    }

    UIElement::update(delta_time, context);
}

void UILabel::renderSelf(Context& /*context*/) {
    if (text_.empty()) {
        return;
    }

    const Vector2f position = getScreenPosition();
    const Color text_color = overrides_.color.value_or(Color::white());
    const std::optional<Color> shadow_color = overrides_.shadow_enabled
        ? std::optional<Color>(overrides_.shadow_color.value_or(Color{0.0f, 0.0f, 0.0f, 0.5f}))
        : std::nullopt;
    const Vector2f shadow_offset = overrides_.shadow_enabled ? overrides_.shadow_offset.value_or(Vector2f{1.0f, 1.0f}) : Vector2f{0.0f, 0.0f};
    text_renderer_.drawText(text_, position, text_color, static_cast<float>(font_size_), shadow_offset, shadow_color);
}

void UILabel::setText(std::string_view text)
{
    text_ = text;
    refreshSize();
}

void UILabel::setFontPath(std::string_view font_path)
{
    font_path_ = font_path;
    font_id_ = entt::hashed_string{font_path_.c_str(), font_path_.size()};
    refreshSize();
}

void UILabel::setFontSize(int font_size)
{
    font_size_ = font_size;
    refreshSize();
}

void UILabel::setStyleKey(std::string_view style_key) {
    if (style_key.empty()) {
        style_id_ = entt::null;
    } else {
        style_id_ = entt::hashed_string{style_key.data(), style_key.size()};
    }
    refreshSize();
}

void UILabel::setTextColor(Color text_color) {
    overrides_.color = text_color;
}

void UILabel::setShadowColor(Color shadow_color) {
    overrides_.shadow_color = shadow_color;
}

void UILabel::setShadowOffset(Vector2f shadow_offset) {
    overrides_.shadow_offset = shadow_offset;
}

void UILabel::setShadowEnabled(bool enabled) {
    overrides_.shadow_enabled = enabled;
}

void UILabel::clearOverrides() {
    const bool layout_might_change = overrides_.glyph_scale.has_value();
    overrides_ = TextRenderOverrides{};
    if (layout_might_change) {
        refreshSize();
    }
}

void UILabel::refreshSize() {
    const auto layout = resolveLabelLayout(text_renderer_, style_id_, overrides_);
    setSize(text_renderer_.getTextSize(text_, font_id_, font_size_, font_path_, &layout));
}

} // namespace dodoe


