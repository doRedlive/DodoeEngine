#include "ui_grid_layout.h"
#include <glm/geometric.hpp>

namespace dodoe {

UIGridLayout::UIGridLayout(Vector2f position, Vector2f size)
    : UILayout(position, size) {
}

void UIGridLayout::setColumnCount(int count) {
    if (column_count_ != count && count > 0) {
        column_count_ = count;
        invalidateLayout();
    }
}

void UIGridLayout::setSpacing(Vector2f spacing) {
    if (glm::distance(spacing_, spacing) > 0.001f) {
        spacing_ = spacing;
        invalidateLayout();
    }
}

void UIGridLayout::setCellSize(Vector2f size) {
    if (glm::distance(cell_size_, size) > 0.001f) {
        cell_size_ = size;
        invalidateLayout();
    }
}

void UIGridLayout::onLayout() {
    if (m_children.empty()) return;

    Vector2f start_offset = {m_padding.left, m_padding.top};
    
    int current_col = 0;
    int current_row = 0;

    for (auto& child : m_children) {
        if (!child->isVisible()) continue;

        // 确定当前子元素大小（优先使用固定 Cell Size）
        Vector2f size;
        if (cell_size_.x > 0.0f && cell_size_.y > 0.0f) {
            size = cell_size_;
            if (glm::distance(child->getRequestedSize(), cell_size_) > 0.001f) {
                child->setSize(cell_size_);
            }
        } else {
            size = child->getRequestedSize();
        }

        // 计算位置
        float x = start_offset.x + current_col * (size.x + spacing_.x);
        float y = start_offset.y + current_row * (size.y + spacing_.y);

        Vector2f new_pos = {x, y};
        if (glm::distance(child->getPosition(), new_pos) > 0.001f) {
            child->setPosition(new_pos);
        }

        // 更新行列索引
        current_col++;
        if (current_col >= column_count_) {
            current_col = 0;
            current_row++;
        }
    }
}

} // namespace dodoe

