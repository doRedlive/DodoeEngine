#pragma once

// do@Redlive

#include "dopch.h"
#include "ui_element.h"
#include "state/ui_state.h"
#include "behavior/interaction_behavior.h"
#include "engine/render/image.h"

namespace dodoe {

    inline constexpr identifier UI_IMAGE_NORMAL_ID = entt::hashed_string{"normal"}.value();
    inline constexpr identifier UI_IMAGE_HOVER_ID = entt::hashed_string{"hover"}.value();
    inline constexpr identifier UI_IMAGE_PRESSED_ID = entt::hashed_string{"pressed"}.value();
    inline constexpr identifier UI_IMAGE_DISABLED_ID = entt::hashed_string{"disabled"}.value();

    inline constexpr identifier UI_SOUND_EVENT_HOVER_ID = UI_IMAGE_HOVER_ID;
    inline constexpr identifier UI_SOUND_EVENT_CLICK_ID = entt::hashed_string{"click"}.value();

    class UIInteractive : public UIElement {
    protected:
        engine::core::Context& m_context;
        Scope<UIState> m_state;
        Scope<UIState> m_next_state;
        std::unordered_map<identifier, engine::render::Image> m_images;
        std::unordered_map<identifier, identifier> m_sound_overrides;
        identifier m_current_image_id = entt::null;
        bool m_interactive = true;
        std::vector<Scope<InteractionBehavior>> m_behaviors;
        bool m_is_pressed{false};
        bool m_is_dragging{false};
        Vector2f m_last_mouse_pos{0.0f, 0.0f};

    public:
        UIInteractive(engine::core::Context& context, Vector2f position = {0.0f, 0.0f}, Vector2f size = {0.0f, 0.0f});
        ~UIInteractive() override;

        virtual void clicked() {}
        virtual void hover_enter() {}
        virtual void hover_leave() {}

        void addImage(identifier name_id, engine::render::Image image);
        void setCurrentImage(identifier name_id);
        virtual void applyStateVisual(identifier state_id);

        void setSoundEvent(identifier event_id, identifier sound_id, std::string_view path = "");
        void setSoundEvent(identifier event_id, std::string_view path);
        void disableSoundEvent(identifier event_id);
        void clearSoundEventOverride(identifier event_id);
        void clearSoundOverrides();
        void playSoundEvent(identifier event_id);

        void setHoverSound(identifier id, std::string_view path = "") { setSoundEvent(UI_SOUND_EVENT_HOVER_ID, id, path); }
        void setClickSound(identifier id, std::string_view path = "") { setSoundEvent(UI_SOUND_EVENT_CLICK_ID, id, path); }
        void disableHoverSound() { disableSoundEvent(UI_SOUND_EVENT_HOVER_ID); }
        void disableClickSound() { disableSoundEvent(UI_SOUND_EVENT_CLICK_ID); }
        void clearHoverSoundOverride() { clearSoundEventOverride(UI_SOUND_EVENT_HOVER_ID); }
        void clearClickSoundOverride() { clearSoundEventOverride(UI_SOUND_EVENT_CLICK_ID); }

        engine::core::Context& getContext() const { return m_context; }
        void setState(Scope<UIState> state);
        void setNextState(Scope<UIState> state);
        UIState* getState() const { return m_state.get(); }

        void setInteractive(bool interactive) { m_interactive = interactive; }
        bool isInteractive() const { return m_interactive; }

        InteractionBehavior* addBehavior(Scope<InteractionBehavior> behavior);
        void clearBehaviors() { m_behaviors.clear(); }

        Vector2f screenToLocal(const Vector2f& screen_pos) const;
        void setPositionByScreen(const Vector2f& screen_pos);

        bool isHovered() const { return m_state && m_state->isHovered(); }
        bool isPressed() const { return m_state && m_state->isPressed(); }
        bool isDragging() const { return m_is_dragging; }

        void mouseEnter();
        void mouseExit();
        void mousePressed();
        void mouseReleased(bool is_inside);

        void update(float delta_time, engine::core::Context& context) override;
    protected:
        void renderSelf(engine::core::Context& context) override;
    };

} // namespace dodoe
