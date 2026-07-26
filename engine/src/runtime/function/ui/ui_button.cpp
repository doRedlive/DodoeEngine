#include "ui_button.h"

#include "state/ui_normal_state.h"
#include "ui_preset_manager.h"

#include "ui_compat.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/core/utils/util.h"

#include <entt/core/hashed_string.hpp>
#include <algorithm>
#include <utility>

namespace dodoe {

namespace {

[[nodiscard]] bool isValidPresetImage(const engine::render::Image& image) {
    return image.getTextureId() != entt::null || !image.getTexturePath().empty();
}

[[nodiscard]] const engine::render::Image* resolvePresetImageForState(const UIButtonSkin& skin, UIButtonVisualState state) {
    auto getImage = [](const std::optional<engine::render::Image>& image) -> const engine::render::Image* {
        if (image && isValidPresetImage(*image)) {
            return &*image;
        }
        return nullptr;
    };

    const engine::render::Image* resolved = nullptr;
    switch (state) {
        case UIButtonVisualState::Normal:
            resolved = getImage(skin.normal_image);
            break;
        case UIButtonVisualState::Hover:
            resolved = getImage(skin.hover_image);
            break;
        case UIButtonVisualState::Pressed:
            resolved = getImage(skin.pressed_image);
            break;
        case UIButtonVisualState::Disabled:
            resolved = getImage(skin.disabled_image);
            break;
        default:
            resolved = nullptr;
            break;
    }

    if (!resolved && state != UIButtonVisualState::Normal) {
        resolved = getImage(skin.normal_image);
    }
    return resolved;
}

[[nodiscard]] Vector4f resolveImageUvRect(const engine::render::Image& image, engine::core::Context& context) {
    Vector4f uv{0.0f, 0.0f, 1.0f, 1.0f};
    const auto source_rect = image.getSourceRect();
    if (source_rect.size.x <= 0.0f || source_rect.size.y <= 0.0f) {
        return uv;
    }

    const Vector2f texture_size = context.getResourceManager().getTextureSize(image.getTextureId(), image.getTexturePath());
    if (texture_size.x <= 0.0f || texture_size.y <= 0.0f) {
        return uv;
    }

    uv = {
        source_rect.pos.x / texture_size.x,
        source_rect.pos.y / texture_size.y,
        (source_rect.pos.x + source_rect.size.x) / texture_size.x,
        (source_rect.pos.y + source_rect.size.y) / texture_size.y
    };

    if (image.isFlipped()) {
        std::swap(uv.x, uv.z);
    }

    return uv;
}

struct ResolvedLabelVisual {
    Color color{Color::white()};
    Vector2f offset{0.0f, 0.0f};
};

[[nodiscard]] ResolvedLabelVisual resolvePresetLabelVisual(const UIButtonSkin& skin, UIButtonVisualState state) {
    ResolvedLabelVisual result{};
    if (!skin.normal_label) {
        return result;
    }

    result.color = skin.normal_label->color;
    result.offset = skin.normal_label->offset;

    auto applyOverrides = [&result](const std::optional<UIButtonLabelOverrides>& overrides) -> bool {
        if (!overrides) {
            return false;
        }
        if (overrides->color) {
            result.color = *overrides->color;
        }
        if (overrides->offset) {
            result.offset = *overrides->offset;
        }
        return true;
    };

    if (state == UIButtonVisualState::Hover) {
        if (applyOverrides(skin.hover_label)) {
            return result;
        }
        result.color = Color{
            (std::min)(1.0f, result.color.r * 1.15f),
            (std::min)(1.0f, result.color.g * 1.15f),
            (std::min)(1.0f, result.color.b * 1.15f),
            result.color.a
        };
        return result;
    }

    if (state == UIButtonVisualState::Pressed) {
        if (applyOverrides(skin.pressed_label)) {
            return result;
        }
        result.offset = Vector2f{0.0f, 2.0f};
        return result;
    }

    if (state == UIButtonVisualState::Disabled) {
        if (applyOverrides(skin.disabled_label)) {
            return result;
        }
        const float luminance = (result.color.r + result.color.g + result.color.b) / 3.0f;
        result.color = Color{luminance, luminance, luminance, result.color.a};
        return result;
    }

    return result;
}

} // namespace

Scope<UIButton> UIButton::Create(engine::core::Context& context,
                                 identifier preset_id,
                                 Vector2f position,
                                 Vector2f size,
                                 std::function<void()> click_callback,
                                 std::function<void()> hover_enter_callback,
                                 std::function<void()> hover_leave_callback) {
    auto button = Scope<UIButton>(new UIButton(context,
                                               position,
                                               size,
                                               std::move(click_callback),
                                               std::move(hover_enter_callback),
                                               std::move(hover_leave_callback)));
    if (!button->initFromPreset(preset_id)) {
        return nullptr;
    }
    return button;
}

Scope<UIButton> UIButton::Create(engine::core::Context& context,
                                 std::string_view preset_key,
                                 Vector2f position,
                                 Vector2f size,
                                 std::function<void()> click_callback,
                                 std::function<void()> hover_enter_callback,
                                 std::function<void()> hover_leave_callback) {
    if (preset_key.empty()) {
        DO_ERROR("UIButton preset_key cannot be empty.");
        return nullptr;
    }

    const identifier preset_id = entt::hashed_string{preset_key.data(), preset_key.size()}.value();
    return Create(context,
                 preset_id,
                 position,
                 size,
                 std::move(click_callback),
                 std::move(hover_enter_callback),
                 std::move(hover_leave_callback));
}

UIButton::UIButton(engine::core::Context& context,
                   Vector2f position,
                   Vector2f size,
                   std::function<void()> click_callback,
                   std::function<void()> hover_enter_callback,
                   std::function<void()> hover_leave_callback)
    : UIInteractive(context, position, size),
      m_click_callback(std::move(click_callback)),
      m_hover_enter_callback(std::move(hover_enter_callback)),
      m_hover_leave_callback(std::move(hover_leave_callback)) {}

const UIButtonSkin* UIButton::getPreset() const {
    return m_context.getResourceManager().getUIPresetManager().getButtonPreset(m_preset_id);
}

bool UIButton::initFromPreset(identifier preset_id) {
    m_preset_id = preset_id;

    const auto* preset = getPreset();
    if (!preset) {
        DO_ERROR("UIButton preset not found (id={}).", m_preset_id);
        return false;
    }
    if (!preset->normal_image || !isValidPresetImage(*preset->normal_image)) {
        DO_ERROR("UIButton preset is missing a usable normal image (id={}).", m_preset_id);
        return false;
    }

    clearSoundOverrides();
    for (const auto& [event_id, path] : preset->sound_events) {
        setSoundEvent(event_id, path);
    }

    if (m_label_text.empty() && preset->normal_label && !preset->normal_label->text.empty()) {
        m_label_text = preset->normal_label->text;
    }

    if (m_size.x == 0.0f && m_size.y == 0.0f) {
        Vector2f image_size = preset->normal_image->getSourceRect().size;
        if (image_size.x <= 0.0f || image_size.y <= 0.0f) {
            image_size = m_context.getResourceManager().getTextureSize(
                preset->normal_image->getTextureId(),
                preset->normal_image->getTexturePath());
        }
        if (image_size.x > 0.0f && image_size.y > 0.0f) {
            setSizeInternal(image_size);
        }
    }

    refreshBaseTextSize();
    setState(create_scope<UINormalState>(this));
    return true;
}

void UIButton::update(float delta_time, engine::core::Context& context) {
    UIInteractive::update(delta_time, context);
    refreshBaseTextSizeIfNeeded();
}

void UIButton::applyStateVisual(identifier state_id) {
    if (const auto state = fromStateId(state_id)) {
        m_current_visual_state = *state;
    }
}

void UIButton::setLabelText(String text) {
    m_label_text = std::move(text);
    refreshBaseTextSize();
}

void UIButton::setTextLayoutFixed() {
    m_text_layout_mode = UIButton::TextLayoutMode::Fixed;
}

void UIButton::setTextLayoutScaleToFit(const Thickness& padding) {
    m_text_layout_mode = UIButton::TextLayoutMode::ScaleToFit;
    m_text_padding = padding;
}

void UIButton::refreshBaseTextSizeIfNeeded() {
    if (m_label_text.empty()) {
        return;
    }

    auto& text_renderer = m_context.getTextRenderer();
    const auto revision = text_renderer.getLayoutRevision();
    if (revision != m_last_label_layout_revision) {
        refreshBaseTextSize();
        return;
    }

    const auto* preset = getPreset();
    if (!preset || !preset->normal_label) {
        return;
    }

    const auto& label = *preset->normal_label;
    const identifier font_id = entt::hashed_string{label.font_path.c_str(), label.font_path.size()}.value();
    if (font_id != m_label_font_id || label.font_size != m_label_font_size) {
        refreshBaseTextSize();
    }
}

void UIButton::refreshBaseTextSize() {
    auto& text_renderer = m_context.getTextRenderer();
    m_last_label_layout_revision = text_renderer.getLayoutRevision();

    m_base_text_size = {0.0f, 0.0f};
    m_label_font_id = entt::null;
    m_label_font_size = 0;

    if (m_label_text.empty()) {
        return;
    }

    const auto* preset = getPreset();
    if (!preset || !preset->normal_label) {
        return;
    }

    const auto& label = *preset->normal_label;
    if (label.font_path.empty() || label.font_size <= 0) {
        return;
    }

    m_label_font_id = entt::hashed_string{label.font_path.c_str(), label.font_path.size()}.value();
    if (m_label_font_id == entt::null) {
        return;
    }
    m_label_font_size = label.font_size;

    const auto default_style = text_renderer.getDefaultUIStyleId();
    const auto layout = text_renderer.getTextStyle(default_style).layout;
    m_base_text_size = text_renderer.getTextSize(m_label_text, m_label_font_id, m_label_font_size, label.font_path, &layout);
}

std::optional<UIButtonVisualState> UIButton::fromStateId(identifier state_id) {
    if (state_id == UI_IMAGE_NORMAL_ID) return UIButtonVisualState::Normal;
    if (state_id == UI_IMAGE_HOVER_ID) return UIButtonVisualState::Hover;
    if (state_id == UI_IMAGE_PRESSED_ID) return UIButtonVisualState::Pressed;
    if (state_id == UI_IMAGE_DISABLED_ID) return UIButtonVisualState::Disabled;
    return std::nullopt;
}

void UIButton::renderSelf(engine::core::Context& context) {
    const Vector2f size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    const auto* skin = getPreset();
    if (!skin) {
        return;
    }

    const auto* image_to_draw = resolvePresetImageForState(*skin, m_current_visual_state);
    if (!image_to_draw) {
        return;
    }

    (void)size; (void)context; (void)image_to_draw;
}

void UIButton::renderLabel(engine::core::Context& context, const UIButtonSkin& skin,
                           const Vector2f& position, const Vector2f& size) {
    if (m_label_text.empty() || !skin.normal_label || m_label_font_id == entt::null || m_label_font_size <= 0) {
        return;
    }

    const auto visual = resolvePresetLabelVisual(skin, m_current_visual_state);

    Vector2f draw_position{0.0f, 0.0f};
    Vector2f text_size = m_base_text_size;
    bool can_draw_text = m_base_text_size.x > 0.0f && m_base_text_size.y > 0.0f;

    if (m_text_layout_mode == UIButton::TextLayoutMode::ScaleToFit) {
        Vector2f available{
            (std::max)(0.0f, size.x - m_text_padding.width()),
            (std::max)(0.0f, size.y - m_text_padding.height())
        };
        float text_scale = 1.0f;
        if (available.x <= 0.0f || available.y <= 0.0f || !can_draw_text) {
            can_draw_text = false;
        } else {
            text_scale = (std::min)(available.x / m_base_text_size.x, available.y / m_base_text_size.y);
            if (text_scale <= 0.0f) {
                can_draw_text = false;
            } else {
                text_size = m_base_text_size * text_scale;
                Vector2f content_origin = position + Vector2f{m_text_padding.left, m_text_padding.top};
                Vector2f centered_offset{
                    (std::max)(0.0f, (available.x - text_size.x) * 0.5f),
                    (std::max)(0.0f, (available.y - text_size.y) * 0.5f)
                };
                draw_position = content_origin + centered_offset + visual.offset;
            }
        }

        if (can_draw_text) {
            const float base_font_size = m_label_font_size > 0 ? static_cast<float>(m_label_font_size) : 0.0f;
            const float scaled_font_size = base_font_size > 0.0f ? base_font_size * text_scale : 0.0f;
            context.getTextRenderer().drawText(m_label_text, draw_position, visual.color, scaled_font_size, Vector2f{0.0f, 0.0f});
            return;
        }
    } else {
        if (can_draw_text) {
            Vector2f centered_offset{
                (std::max)(0.0f, (size.x - text_size.x) * 0.5f),
                (std::max)(0.0f, (size.y - text_size.y) * 0.5f)
            };
            draw_position = position + centered_offset + visual.offset;
        }
    }

    if (!can_draw_text) {
        return;
    }

    const float font_size = m_label_font_size > 0 ? static_cast<float>(m_label_font_size) : 0.0f;
    context.getTextRenderer().drawText(m_label_text, draw_position, visual.color, font_size, Vector2f{0.0f, 0.0f});
}

} // namespace dodoe


