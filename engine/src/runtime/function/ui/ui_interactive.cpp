#include "ui_interactive.h"
#include "state/ui_state.h"
#include "ui_imgui_utils.h"
#include "runtime/core/math/math.h"
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace dodoe {

namespace {

[[nodiscard]] identifier getDefaultSoundForEvent(identifier event_id) {
    if (event_id == UI_SOUND_EVENT_HOVER_ID) {
        return "ui_hover"_hs;
    }
    if (event_id == UI_SOUND_EVENT_CLICK_ID) {
        return "ui_click"_hs;
    }
    return entt::null;
}

} // namespace

UIInteractive::~UIInteractive() = default;

UIInteractive::UIInteractive(Context &context, Vector2f position, Vector2f size)
    : UIElement(std::move(position), std::move(size)), m_context(context)
{
    DO_TRACE("UIInteractive created.");
    m_last_mouse_pos = m_context.getInputManager().getLogicalMousePosition();
}

void UIInteractive::setState(Scope<UIState> state)
{
    if (!state) {
        DO_WARN("UIInteractive received a null state.");
        return;
    }

    m_state = std::move(state);
    m_state->enter();
}

void UIInteractive::setNextState(Scope<UIState> state)
{
    m_next_state = std::move(state);
}

void UIInteractive::addImage(identifier name_id, Image image)
{
    if (m_size.x == 0.0f && m_size.y == 0.0f) {
        auto texture_size = m_context.getResourceManager().getTextureSize(image.getTextureId(), image.getTexturePath());
        setSizeInternal(texture_size);
    }
    m_images.insert_or_assign(name_id, std::move(image));
}

void UIInteractive::setCurrentImage(identifier name_id)
{
    if (!m_images.contains(name_id)) {
        DO_WARN("Image '{}' not found.", name_id);
        return;
    }
    m_current_image_id = name_id;
}

void UIInteractive::applyStateVisual(identifier state_id)
{
    setCurrentImage(state_id);
}

void UIInteractive::setSoundEvent(identifier event_id, identifier sound_id, std::string_view path)
{
    if (event_id == entt::null) {
        return;
    }

    if (!path.empty() && sound_id != entt::null) {
        m_context.getResourceManager().loadSound(sound_id, path);
    }
    m_sound_overrides.insert_or_assign(event_id, sound_id);
}

void UIInteractive::setSoundEvent(identifier event_id, std::string_view path)
{
    if (path.empty()) {
        disableSoundEvent(event_id);
        return;
    }

    const identifier sound_id = entt::hashed_string{path.data(), path.size()}.value();
    setSoundEvent(event_id, sound_id, path);
}

void UIInteractive::disableSoundEvent(identifier event_id)
{
    if (event_id == entt::null) {
        return;
    }
    m_sound_overrides.insert_or_assign(event_id, entt::null);
}

void UIInteractive::clearSoundEventOverride(identifier event_id)
{
    m_sound_overrides.erase(event_id);
}

void UIInteractive::clearSoundOverrides()
{
    m_sound_overrides.clear();
}

void UIInteractive::playSoundEvent(identifier event_id)
{
    if (event_id == entt::null) {
        return;
    }

    if (auto it = m_sound_overrides.find(event_id); it != m_sound_overrides.end()) {
        if (it->second == entt::null) {
            return;
        }
        if (!m_context.getAudioPlayer().playSound(it->second)) {
            DO_WARN("Sound '{}' not found or failed to play.", it->second);
        }
        return;
    }

    const identifier default_sound = getDefaultSoundForEvent(event_id);
    if (default_sound == entt::null) {
        return;
    }

    if (!m_context.getAudioPlayer().playSound(default_sound)) {
        DO_TRACE("Sound '{}' not found or failed to play.", default_sound);
    }
}

void UIInteractive::update(float delta_time, Context &context)
{
    UIElement::update(delta_time, context);

    if (m_state && m_interactive) {
        if (m_next_state) {
            setState(std::move(m_next_state));
        }
        m_state->update(delta_time, context);
    }

    if (m_interactive && m_is_pressed) {
        const Vector2f current = m_context.getInputManager().getLogicalMousePosition();
        const Vector2f delta = current - m_last_mouse_pos;
        if (Math::Length(delta) > 0.0f) {
            m_is_dragging = true;
            for (auto& behavior : m_behaviors) {
                if (behavior) {
                    behavior->onDragUpdate(*this, current, delta);
                }
            }
            m_last_mouse_pos = current;
        }
    }
}

void UIInteractive::renderSelf(Context &context)
{
    if (m_current_image_id == entt::null) {
        return;
    }

    auto it = m_images.find(m_current_image_id);
    if (it == m_images.end()) {
        return;
    }

    const auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        DO_WARN("UIInteractive has invalid size ({}, {}).", size.x, size.y);
        return;
    }

    ui::drawImage(it->second, getScreenPosition(), size);
}

void UIInteractive::mouseEnter()
{
    if (!m_interactive) return;
    if (m_state) m_state->onMouseEnter();
    for (auto& behavior : m_behaviors) {
        if (behavior) {
            behavior->onHoverEnter(*this);
        }
    }
}

void UIInteractive::mouseExit()
{
    if (!m_interactive) return;
    if (m_state) m_state->onMouseExit();
    for (auto& behavior : m_behaviors) {
        if (behavior) {
            behavior->onHoverExit(*this);
        }
    }
}

void UIInteractive::mousePressed()
{
    if (!m_interactive) return;
    m_is_pressed = true;
    m_last_mouse_pos = m_context.getInputManager().getLogicalMousePosition();
    if (m_state) m_state->onMousePressed();
    for (auto& behavior : m_behaviors) {
        if (behavior) {
            behavior->onPressed(*this);
            behavior->onDragBegin(*this, m_last_mouse_pos);
        }
    }
}

void UIInteractive::mouseReleased(bool is_inside)
{
    if (!m_interactive) return;
    const Vector2f current = m_context.getInputManager().getLogicalMousePosition();
    for (auto& behavior : m_behaviors) {
        if (behavior) {
            behavior->onDragEnd(*this, current, is_inside);
        }
    }
    m_is_dragging = false;
    m_is_pressed = false;
    if (m_state) m_state->onMouseReleased(is_inside);
    if (is_inside) {
        for (auto& behavior : m_behaviors) {
            if (behavior) {
                behavior->onClick(*this);
            }
        }
    }
}

InteractionBehavior* UIInteractive::addBehavior(Scope<InteractionBehavior> behavior)
{
    if (!behavior) {
        return nullptr;
    }
    behavior->onAttach(*this);
    m_behaviors.push_back(std::move(behavior));
    return m_behaviors.back().get();
}

Vector2f UIInteractive::screenToLocal(const Vector2f& screen_pos) const {
    if (m_parent) {
        const auto parent_content = m_parent->getContentBounds();
        return screen_pos - parent_content.pos;
    }
    return screen_pos;
}

void UIInteractive::setPositionByScreen(const Vector2f& screen_pos) {
    setPosition(screenToLocal(screen_pos));
}

} // namespace dodoe


