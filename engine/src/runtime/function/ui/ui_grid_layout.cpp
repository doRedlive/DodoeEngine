// do@Redlive

#include "ui_grid_layout.h"

namespace dodoe {

    void UIGridLayout::onLayout() {
        const auto& children = getChildren();
        if (children.empty() || m_columns <= 0) return;

        Int col = 0;
        Int row = 0;

        for (auto& child : children) {
            if (!child->isVisible()) continue;

            Float x = col * (m_cell_size.x + m_spacing.x);
            Float y = row * (m_cell_size.y + m_spacing.y);
            child->setPosition({x, y});
            child->setSize(m_cell_size);

            ++col;
            if (col >= m_columns) {
                col = 0;
                ++row;
            }
        }
    }

} // namespace dodoe
