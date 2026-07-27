// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"

namespace dodoe {

    class UIInteractive : public UIElement {
    private:
        Bool m_interactable{true};
        Bool m_raycast_target{true};
        Bool m_is_hovered{false};
        Bool m_is_pressed{false};
        Bool m_clicked{false};
        Bool m_entered{false};
        Bool m_exited{false};

    public:
        std::function<void()> on_click;
        std::function<void()> on_hover_enter;
        std::function<void()> on_hover_leave;
        std::function<void(Bool)> on_press_changed;
        std::function<void(Float)> on_scroll;

        [[nodiscard]] Bool isHovered() const { return m_is_hovered; }
        [[nodiscard]] Bool isPressed() const { return m_is_pressed; }
        [[nodiscard]] Bool isInteractable() const { return m_interactable; }
        void setInteractable(Bool interactable) { m_interactable = interactable; }

        [[nodiscard]] Bool isRaycastTarget() const { return m_raycast_target; }
        void setRaycastTarget(Bool enabled) { m_raycast_target = enabled; }

        [[nodiscard]] Bool pollClicked() { Bool v = m_clicked; m_clicked = false; return v; }
        [[nodiscard]] Bool pollEntered() { Bool v = m_entered; m_entered = false; return v; }
        [[nodiscard]] Bool pollExited() { Bool v = m_exited; m_exited = false; return v; }

    protected:
        virtual void onMouseEnter();
        virtual void onMouseExit();
        virtual void onMouseDown();
        virtual void onMouseUp(Bool isInside);
        virtual void onScroll(Float delta);

        friend class UIInputRouter;
    };

} // namespace dodoe
