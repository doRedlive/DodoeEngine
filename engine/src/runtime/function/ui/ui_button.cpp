// do@Redlive

#include "ui_button.h"
#include "ui_image.h"
#include "ui_label.h"
#include "ui_render_batch.h"
#include "ui_preset_manager.h"

namespace dodoe {

    String UIButton::getLabel() const {
        return m_label ? m_label->getText() : String();
    }

    void UIButton::setLabel(const String& text) {
        if (m_label) {
            m_label->setText(text);
        }
    }

    void UIButton::setStateImage(ButtonState state, Texture2D* texture, Rect uv) {
        auto& vis = m_visuals[static_cast<int>(state)];
        vis.texture = texture;
        vis.uv_rect = uv;
    }

    void UIButton::setStateColor(ButtonState state, Color color) {
        auto& vis = m_visuals[static_cast<int>(state)];
        vis.color = color;
    }

    void UIButton::applyPreset(const ButtonPreset& preset) {
        setStateImage(ButtonState::Normal, preset.normal_texture, {0, 0, 1, 1});
        setStateColor(ButtonState::Normal, preset.normal_color);
        setStateImage(ButtonState::Hovered, preset.hovered_texture, {0, 0, 1, 1});
        setStateColor(ButtonState::Hovered, preset.hovered_color);
        setStateImage(ButtonState::Pressed, preset.pressed_texture, {0, 0, 1, 1});
        setStateColor(ButtonState::Pressed, preset.pressed_color);
        m_preset_id = preset.id;

        applyVisuals();
    }

    void UIButton::onCollectRenderData(UIRenderBatch& batch) {
        if (!isVisible()) return;
        UIElement::onCollectRenderData(batch);
    }

    void UIButton::applyVisuals() {
        auto& vis = m_visuals[static_cast<int>(m_state)];
        if (m_icon) {
            m_icon->setTexture(vis.texture);
            m_icon->setUVRect(vis.uv_rect);
            m_icon->setColor(vis.color);
        }
        if (m_label) {
            m_label->setColor(vis.color);
        }
    }

    void UIButton::transitionTo(ButtonState state) {
        if (m_state == state) return;
        m_state = state;
        applyVisuals();
    }

    void UIButton::onMouseEnter() {
        UIInteractive::onMouseEnter();
        transitionTo(ButtonState::Hovered);
    }

    void UIButton::onMouseExit() {
        UIInteractive::onMouseExit();
        transitionTo(ButtonState::Normal);
    }

    void UIButton::onMouseDown() {
        UIInteractive::onMouseDown();
        transitionTo(ButtonState::Pressed);
    }

    void UIButton::onMouseUp(Bool isInside) {
        UIInteractive::onMouseUp(isInside);
        transitionTo(isInside ? ButtonState::Hovered : ButtonState::Normal);
    }

} // namespace dodoe
