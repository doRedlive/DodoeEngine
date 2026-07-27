// do@Redlive

#include "ui_element.h"
#include "ui_render_batch.h"

namespace dodoe {

    void UIElement::addChild(Scope<UIElement> child) {
        if (!child) return;
        child->m_parent = this;
        child->invalidateLayout();
        m_children.push_back(std::move(child));
    }

    Scope<UIElement> UIElement::removeChild(UIElement* child) {
        if (!child) return nullptr;
        auto it = std::find_if(m_children.begin(), m_children.end(),
            [child](const Scope<UIElement>& c) { return c.get() == child; });
        if (it == m_children.end()) return nullptr;
        auto removed = std::move(*it);
        m_children.erase(it);
        removed->m_parent = nullptr;
        invalidateLayout(false);
        return removed;
    }

    UIElement* UIElement::findChildById(identifier id) const {
        for (const auto& child : m_children) {
            if (child->m_id == id) return child.get();
            if (auto* found = child->findChildById(id)) return found;
        }
        return nullptr;
    }

    void UIElement::setAnchor(Vector2f min, Vector2f max) {
        m_anchor_min = min;
        m_anchor_max = max;
        invalidateLayout();
    }

    void UIElement::setPivot(Vector2f pivot) {
        m_pivot = pivot;
        invalidateLayout();
    }

    void UIElement::setSize(Vector2f size) {
        m_size = size;
        invalidateLayout();
    }

    void UIElement::setPosition(Vector2f position) {
        m_position = position;
        invalidateLayout();
    }

    void UIElement::setPadding(const Thickness& padding) {
        m_padding = padding;
        invalidateLayout();
    }

    void UIElement::setMargin(const Thickness& margin) {
        m_margin = margin;
        invalidateLayout();
    }

    Vector2f UIElement::getScreenPosition() const {
        ensureLayout();
        return m_cached_screen_pos;
    }

    Vector2f UIElement::getLayoutSize() const {
        ensureLayout();
        return m_cached_layout_size;
    }

    Rect UIElement::getScreenRect() const {
        ensureLayout();
        return Rect(m_cached_screen_pos, m_cached_layout_size);
    }

    Bool UIElement::hitTest(Vector2f localPos) const {
        ensureLayout();
        return localPos.x >= 0 && localPos.x <= m_cached_layout_size.x &&
               localPos.y >= 0 && localPos.y <= m_cached_layout_size.y;
    }

    void UIElement::invalidateLayout(Bool propagate) {
        m_layout_dirty = true;
        if (propagate && m_parent) {
            m_parent->invalidateLayout(true);
        }
        for (auto& child : m_children) {
            child->m_layout_dirty = true;
        }
    }

    void UIElement::ensureLayout() const {
        if (!m_layout_dirty) return;

        Vector2f parent_pos{0, 0};
        Vector2f parent_size{0, 0};

        if (m_parent) {
            m_parent->ensureLayout();
            parent_pos = m_parent->m_cached_screen_pos;
            parent_size = m_parent->m_cached_layout_size;

            parent_pos.x += m_parent->m_padding.left;
            parent_pos.y += m_parent->m_padding.top;
            parent_size.x -= (m_parent->m_padding.left + m_parent->m_padding.right);
            parent_size.y -= (m_parent->m_padding.top + m_parent->m_padding.bottom);
        }

        Vector2f layout_size = m_size;
        if (m_anchor_min.x != m_anchor_max.x) {
            layout_size.x = parent_size.x * (m_anchor_max.x - m_anchor_min.x) + m_size.x;
        }
        if (m_anchor_min.y != m_anchor_max.y) {
            layout_size.y = parent_size.y * (m_anchor_max.y - m_anchor_min.y) + m_size.y;
        }

        Vector2f screen_pos;
        screen_pos.x = parent_pos.x + parent_size.x * m_anchor_min.x + m_position.x - layout_size.x * m_pivot.x;
        screen_pos.y = parent_pos.y + parent_size.y * m_anchor_min.y + m_position.y - layout_size.y * m_pivot.y;

        m_cached_screen_pos = screen_pos;
        m_cached_layout_size = layout_size;
        m_layout_dirty = false;

        const_cast<UIElement*>(this)->onLayout();
    }

    void UIElement::onCollectRenderData(UIRenderBatch& batch) {
        if (!m_visible) return;
        for (auto& child : m_children) {
            if (child->isVisible()) {
                child->onCollectRenderData(batch);
            }
        }
    }

} // namespace dodoe
