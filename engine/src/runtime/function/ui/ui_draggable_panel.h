#pragma once

#include "dopch.h"
#include "ui_interactive.h"
#include "ui_panel.h"
#include "behavior/drag_behavior.h"

namespace dodoe {

class UIDraggablePanel final : public UIInteractive {
private:
    dodoe::Scope<UIPanel> content_panel_;
    UIPanel* content_panel_ptr_{nullptr};
    dodoe::Scope<DragBehavior> drag_behavior_;
    Vector2f drag_offset_{0.0f, 0.0f};

public:
    UIDraggablePanel(engine::core::Context& context,
                     Vector2f position,
                     Vector2f size,
                     std::optional<Color> background_color = std::nullopt,
                     std::optional<engine::render::Image> skin_image = std::nullopt,
                     std::optional<engine::render::NineSliceMargins> skin_margins = std::nullopt);

    UIPanel* getContentPanel() const { return content_panel_ptr_; }

    void setSkinImage(engine::render::Image image) { if (content_panel_ptr_) content_panel_ptr_->setSkinImage(std::move(image)); }
    void setNineSliceMargins(std::optional<engine::render::NineSliceMargins> margins) { if (content_panel_ptr_) content_panel_ptr_->setNineSliceMargins(std::move(margins)); }
    void setPadding(const Thickness& padding) { if (content_panel_ptr_) content_panel_ptr_->setPadding(padding); }
    void setBackgroundColor(std::optional<Color> color) { if (content_panel_ptr_) content_panel_ptr_->setBackgroundColor(std::move(color)); }
    void clearSkinImage() { if (content_panel_ptr_) content_panel_ptr_->clearSkinImage(); }
    bool hasNineSliceSkin() const { return content_panel_ptr_ && content_panel_ptr_->hasNineSliceSkin(); }
    const engine::render::NineSliceMargins* getNineSliceMargins() const { return content_panel_ptr_ ? content_panel_ptr_->getNineSliceMargins() : nullptr; }
    void markNineSliceDirty() { if (content_panel_ptr_) content_panel_ptr_->markNineSliceDirty(); }

private:
    void setTopLeftByScreen(const Vector2f& screen_pos);
};

} // namespace dodoe
