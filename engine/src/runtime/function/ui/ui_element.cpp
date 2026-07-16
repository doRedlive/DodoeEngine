#include "ui_element.h"
#include "ui_interactive.h"
#include "ui_imgui_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace dodoe {

UIElement::UIElement(Vector2f position, Vector2f size)
    : m_position(std::move(position)), m_size(std::move(size)) {
    m_layout_position = m_position;
    m_layout_size = m_size;
}

void UIElement::update(float delta_time, Context& context) {
    ensureLayout();

    if (!m_visible) return; 

    for (auto it = m_children.begin(); it != m_children.end();) {
        if (*it && !(*it)->isNeedRemove()) {
            (*it)->update(delta_time, context);
            ++it;
        } else {
            it = m_children.erase(it);
        }
    }
}

void UIElement::render(Context& context) {
    ensureLayout();

    if (!m_visible) return;

    renderSelf(context);
    for (const auto& child : m_children) {
        if (child) child->render(context);
    }
}

void UIElement::addChild(Scope<UIElement> child, int order_index) {
    if (child) {
        child->setParent(this);
        if (order_index >= 0) {
            child->setOrderIndex(order_index);
        }
        m_children.push_back(std::move(child));
        sortChildrenByOrderIndex();
        invalidateLayout();
    }
}

Scope<UIElement> UIElement::removeChild(UIElement* child_ptr) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
                           [child_ptr](const Scope<UIElement>& p) {
                                return p.get() == child_ptr; 
                           });

    if (it != m_children.end()) {
        Scope<UIElement> removed_child = extract_scope(m_children, it);
        removed_child->setParent(nullptr);
        invalidateLayout();
        return removed_child;
    }
    return nullptr;
}

Scope<UIElement> UIElement::removeChildById(identifier id) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
                           [id](const Scope<UIElement>& p) {
                                return p->getId() == id;
                           });
    if (it != m_children.end()) {
        Scope<UIElement> removed_child = extract_scope(m_children, it);
        removed_child->setParent(nullptr);
        invalidateLayout();
        return removed_child;
    }
    return nullptr;
}

void UIElement::removeAllChildren() {
    for (auto& child : m_children) {
        child->setParent(nullptr);
    }
    m_children.clear();
    invalidateLayout();
}

UIElement* UIElement::getChildById(identifier id) const {
    auto it = std::find_if(m_children.begin(), m_children.end(),
                          [id](const Scope<UIElement>& p) {
                                return p->getId() == id; 
                           });
    if (it != m_children.end()) {
        return it->get();
    }
    DO_TRACE("UIElement::getChildById: child not found: {}", id);
    return nullptr;
}

Vector2f UIElement::getScreenPosition() const {
    ensureLayout();
    return m_layout_position;
}

void UIElement::sortChildrenByOrderIndex() {
    std::stable_sort(m_children.begin(), m_children.end(), [](const Scope<UIElement>& a, const Scope<UIElement>& b) {
        return a->getOrderIndex() < b->getOrderIndex();
    });
}

void UIElement::setOrderIndex(int order_index) {
    m_order_index = order_index;
    if (m_parent) {
        m_parent->sortChildrenByOrderIndex();
        m_parent->invalidateLayout();
    }
}

Rect UIElement::getBounds() const {
    ensureLayout();
    return Rect(m_layout_position, m_layout_size);
}

bool UIElement::isPointInside(const Vector2f& point) const {
    auto bounds = getBounds();
    return (point.x >= bounds.pos.x && point.x < (bounds.pos.x + bounds.size.x) &&
            point.y >= bounds.pos.y && point.y < (bounds.pos.y + bounds.size.y));
}

Vector2f UIElement::getSize() const {
    return getLayoutSize();
}

Vector2f UIElement::getLayoutSize() const {
    ensureLayout();
    return m_layout_size;
}

Rect UIElement::getContentBounds() const {
    ensureLayout();
    Vector2f content_pos = m_layout_position + Vector2f{m_padding.left, m_padding.top};
    Vector2f content_size = Math::Max(m_layout_size - Vector2f{m_padding.width(), m_padding.height()}, Vector2f{0.0f});
    return {content_pos, content_size};
}

void UIElement::setAnchor(Vector2f anchor_min, Vector2f anchor_max) {
    m_anchor_min = anchor_min;
    m_anchor_max = anchor_max;
    invalidateLayout();
}

void UIElement::setPivot(Vector2f pivot) {
    m_pivot = pivot;
    invalidateLayout(true);
}

void UIElement::setPadding(const Thickness& padding) {
    m_padding = padding;
    invalidateLayout();
}

void UIElement::setMargin(const Thickness& margin) {
    m_margin = margin;
    invalidateLayout();
}

const UIInteractive* UIElement::findInteractiveAt(const Vector2f& point) const {
    if (!m_visible) {
        return nullptr;
    }

    ensureLayout();

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if (*it) {
            if (auto* result = (*it)->findInteractiveAt(point); result) {
                return result;
            }
        }
    }

    const auto* interactive = dynamic_cast<const UIInteractive*>(this);
    if (interactive && interactive->isInteractive() && isPointInside(point)) {
        return interactive;
    }
    return nullptr;
}

UIInteractive* UIElement::findInteractiveAt(const Vector2f& point) {
    return const_cast<UIInteractive*>(std::as_const(*this).findInteractiveAt(point));
}

void UIElement::renderSelf(Context& /*context*/) {
}

void UIElement::invalidateLayout(bool propagate) {
    m_layout_dirty = true;
    if (propagate) {
        for (auto& child : m_children) {
            if (child) {
                child->invalidateLayout(true);
            }
        }
    }
}

void UIElement::ensureLayout() const {
    if (!m_layout_dirty) {
        return;
    }

    if (!m_parent) {
        m_layout_size = m_size;
        m_layout_position = m_position;
        m_layout_dirty = false;
        return;
    }

    auto parent_content = m_parent->getContentBounds();
    Vector2f parent_origin = parent_content.pos;
    Vector2f parent_size = parent_content.size;

    bool stretched = std::fabs(m_anchor_min.x - m_anchor_max.x) > std::numeric_limits<float>::epsilon() ||
                     std::fabs(m_anchor_min.y - m_anchor_max.y) > std::numeric_limits<float>::epsilon();

    Vector2f anchor_min_pos = parent_origin + parent_size * m_anchor_min;
    Vector2f anchor_max_pos = parent_origin + parent_size * m_anchor_max;
    Vector2f available_size = anchor_max_pos - anchor_min_pos;

    Vector2f final_size = m_size;
    if (stretched) {
        final_size = available_size;
        final_size.x = (std::max)(0.0f, final_size.x - m_margin.width());
        final_size.y = (std::max)(0.0f, final_size.y - m_margin.height());
    }
    m_layout_size = final_size;

    Vector2f anchor_reference = anchor_min_pos + m_position;

    Vector2f top_left = anchor_reference + Vector2f{m_margin.left, m_margin.top} - m_layout_size * m_pivot;
    m_layout_position = top_left;

    m_layout_dirty = false;

    const_cast<UIElement*>(this)->onLayout();
}

void UIElement::setParentInternal(UIElement* parent) {
    m_parent = parent;
    invalidateLayout();
}

void UIElement::setSizeInternal(Vector2f size) {
    m_size = std::move(size);
    invalidateLayout();
}

} // namespace dodoe


