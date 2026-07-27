// do@Redlive

#include "ui_stack_layout.h"

namespace dodoe {

    void UIStackLayout::onLayout() {
        const auto& children = getChildren();
        if (children.empty()) return;

        Float offset = 0;
        for (auto& child : children) {
            if (!child->isVisible()) continue;

            Rect child_rect = child->getScreenRect();

            if (m_direction == LayoutDirection::Vertical) {
                child->setPosition({child->getPosition().x, offset});
                offset += child_rect.size.y + m_spacing;
            } else {
                child->setPosition({offset, child->getPosition().y});
                offset += child_rect.size.x + m_spacing;
            }
        }
    }

} // namespace dodoe
