// do@Redlive

#include "ui_input_router.h"
#include "ui_interactive.h"

namespace dodoe {

    UIInteractive* UIInputRouter::hitTest(UIElement* element, Vector2f screen_pos) const {
        if (!element || !element->isVisible()) return nullptr;

        const auto& children = element->getChildren();
        for (Size_t i = children.size(); i > 0; --i) {
            UIInteractive* hit = hitTest(children[i - 1].get(), screen_pos);
            if (hit) return hit;
        }

        auto* interactive = dynamic_cast<UIInteractive*>(element);
        if (!interactive || !interactive->isRaycastTarget() || !interactive->isInteractable()) return nullptr;

        Rect rect = element->getScreenRect();
        Vector2f local = {screen_pos.x - rect.pos.x, screen_pos.y - rect.pos.y};
        if (!element->hitTest(local)) return nullptr;

        return interactive;
    }

    void UIInputRouter::processMouseMove(Vector2f screen_pos) {
        m_last_mouse_pos = screen_pos;

        auto* hit = hitTest(m_root, screen_pos);

        if (hit != m_hovered) {
            if (m_hovered) m_hovered->onMouseExit();
            m_hovered = hit;
            if (m_hovered) m_hovered->onMouseEnter();
        }
    }

    void UIInputRouter::processMouseDown(Vector2f screen_pos) {
        m_last_mouse_pos = screen_pos;
        auto* hit = hitTest(m_root, screen_pos);

        if (hit) {
            m_pressed = hit;
            hit->onMouseDown();
        }
    }

    void UIInputRouter::processMouseUp(Vector2f screen_pos) {
        m_last_mouse_pos = screen_pos;
        auto* hit = hitTest(m_root, screen_pos);

        if (m_pressed) {
            m_pressed->onMouseUp(hit == m_pressed);
            m_pressed = nullptr;
        }
    }

    void UIInputRouter::processScroll(Vector2f screen_pos, Float delta) {
        m_last_mouse_pos = screen_pos;
        auto* hit = hitTest(m_root, screen_pos);
        if (hit) {
            hit->onScroll(delta);
        }
    }

} // namespace dodoe
