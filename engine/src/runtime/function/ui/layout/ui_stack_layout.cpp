#include "ui_stack_layout.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace dodoe {

UIStackLayout::UIStackLayout(Vector2f position, Vector2f size)
    : UILayout(position, size) {
}

void UIStackLayout::setOrientation(Orientation orientation) {
    if (orientation_ != orientation) {
        orientation_ = orientation;
        invalidateLayout();
    }
}

void UIStackLayout::setSpacing(float spacing) {
    if (std::abs(spacing_ - spacing) > 0.001f) {
        spacing_ = spacing;
        invalidateLayout();
    }
}

void UIStackLayout::setContentAlignment(Alignment alignment) {
    if (alignment_ != alignment) {
        alignment_ = alignment;
        invalidateLayout();
    }
}

void UIStackLayout::onLayout() {
    if (m_children.empty()) return;

    Vector2f content_start = {m_padding.left, m_padding.top};
    Vector2f content_size = m_layout_size - Vector2f(m_padding.width(), m_padding.height());
    
    content_size = Math::Max(content_size, Vector2f(0.0f));

    float current_pos = 0.0f;
    bool is_vertical = (orientation_ == Orientation::Vertical);

    float total_content_length = 0.0f;
    for (const auto& child : m_children) {
        if (!child->isVisible()) continue;
        
        Vector2f child_size = child->getRequestedSize(); 
        
        float length = is_vertical ? child_size.y : child_size.x;
        total_content_length += length;
    }
    
    int visible_count = 0;
    for (const auto& child : m_children) {
        if (child->isVisible()) ++visible_count;
    }
    if (visible_count > 1) {
        total_content_length += (visible_count - 1) * spacing_;
    }

    if (is_vertical) {
        if (alignment_ == Alignment::Center) {
            current_pos = (content_size.y - total_content_length) * 0.5f;
        } else if (alignment_ == Alignment::End) {
            current_pos = content_size.y - total_content_length;
        }
    } else {
        if (alignment_ == Alignment::Center) {
            current_pos = (content_size.x - total_content_length) * 0.5f;
        } else if (alignment_ == Alignment::End) {
            current_pos = content_size.x - total_content_length;
        }
    }

    float start_offset_x = content_start.x;
    float start_offset_y = content_start.y;

    for (auto& child : m_children) {
        if (!child->isVisible()) continue;

        Vector2f child_pos = child->getPosition();
        Vector2f child_req_size = child->getRequestedSize();
        
        Vector2f new_pos = child_pos;

        if (is_vertical) {
            new_pos.y = start_offset_y + current_pos;
             new_pos.x = start_offset_x;

            current_pos += child_req_size.y + spacing_;
        } else {
            new_pos.x = start_offset_x + current_pos;
             new_pos.y = start_offset_y;

            current_pos += child_req_size.x + spacing_;
        }

        if (glm::distance(child_pos, new_pos) > 0.001f) {
            child->setPosition(new_pos);
        }
    }

    if (auto_resize_) {
        Vector2f new_size = m_layout_size;
        if (is_vertical) {
            new_size.y = total_content_length + m_padding.top + m_padding.bottom;
        } else {
            new_size.x = total_content_length + m_padding.left + m_padding.right; 
        }
        
        if (glm::distance(m_size, new_size) > 0.001f) {
            setSizeInternal(new_size); // Update self size
        }
    }
}

} // namespace dodoe

