#include "ui_draggable_panel.h"

namespace dodoe {

UIDraggablePanel::UIDraggablePanel(Context& context,
                                   Vector2f position,
                                   Vector2f size,
                                   std::optional<Color> background_color,
                                   std::optional<Image> skin_image,
                                   std::optional<NineSliceMargins> skin_margins)
    : UIInteractive(context, std::move(position), std::move(size)) {
    auto panel = create_scope<UIPanel>(Vector2f{0.0f, 0.0f},
                                       size,
                                       std::move(background_color),
                                       std::move(skin_image),
                                       std::move(skin_margins));
    panel->setAnchor({0, 0}, {1, 1});
    panel->setPivot({0, 0});
    content_panel_ptr_ = panel.get();
    addChild(std::move(panel));

    drag_behavior_ = create_scope<DragBehavior>();
    drag_behavior_->setOnBegin([this](UIInteractive&, const Vector2f& pos) {
        drag_offset_ = pos - getScreenPosition();
    });
    drag_behavior_->setOnUpdate([this](UIInteractive&, const Vector2f& pos, const Vector2f&) {
        const Vector2f target_screen = pos - drag_offset_;
        setTopLeftByScreen(target_screen);
    });
    addBehavior(std::move(drag_behavior_));
}

void UIDraggablePanel::setTopLeftByScreen(const Vector2f& screen_pos) {
    ensureLayout();
    if (!m_parent) {
        setPositionByScreen(screen_pos);
        return;
    }

    const auto parent_content = m_parent->getContentBounds();
    const Vector2f parent_size = parent_content.size;

    Vector2f top_left_local = screen_pos - parent_content.pos;

    Vector2f anchor_min_pos_local = parent_size * m_anchor_min;
    Vector2f margin_offset{m_margin.left, m_margin.top};
    Vector2f desired_position = top_left_local - anchor_min_pos_local - margin_offset + getLayoutSize() * m_pivot;

    setPosition(desired_position);
}

} // namespace dodoe


