// do@Redlive

#include "ui_interactive.h"

namespace dodoe {

    void UIInteractive::onMouseEnter() {
        m_is_hovered = true;
        m_entered = true;
        if (on_hover_enter) on_hover_enter();
    }

    void UIInteractive::onMouseExit() {
        m_is_hovered = false;
        m_exited = true;
        if (on_hover_leave) on_hover_leave();
    }

    void UIInteractive::onMouseDown() {
        m_is_pressed = true;
        if (on_press_changed) on_press_changed(true);
    }

    void UIInteractive::onMouseUp(Bool isInside) {
        m_is_pressed = false;
        if (on_press_changed) on_press_changed(false);
        if (isInside) {
            m_clicked = true;
            if (on_click) on_click();
        }
    }

    void UIInteractive::onScroll(Float delta) {
        if (on_scroll) on_scroll(delta);
    }

} // namespace dodoe
