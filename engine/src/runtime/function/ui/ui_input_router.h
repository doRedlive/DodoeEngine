// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"

namespace dodoe {

    class UIInputRouter {
    private:
        UIElement* m_root{nullptr};
        UIInteractive* m_hovered{nullptr};
        UIInteractive* m_pressed{nullptr};
        Vector2f m_last_mouse_pos{0, 0};

    public:
        void setRoot(UIElement* root) { m_root = root; }

        void processMouseMove(Vector2f screen_pos);
        void processMouseDown(Vector2f screen_pos);
        void processMouseUp(Vector2f screen_pos);
        void processScroll(Vector2f screen_pos, Float delta);

    private:
        [[nodiscard]] UIInteractive* hitTest(UIElement* element, Vector2f screen_pos) const;
    };

} // namespace dodoe
