#include "ui_progress_bar.h"
#include <algorithm>
#include <memory>
#include <string>
#include <glm/geometric.hpp>


namespace dodoe {

UIProgressBar::UIProgressBar(Context& context, Vector2f position, Vector2f size)
    : UIElement(position, size) {

    auto bg = create_scope<UIImage>(Image{});
    bg->setOrderIndex(0);
    bg->setAnchor({0, 0}, {1, 1});
    background_image_ = bg.get();
    addChild(std::move(bg));

    auto fill = create_scope<UIImage>(Image{});
    fill->setOrderIndex(1);
    fill->setAnchor({0, 0}, {1, 1});
    fill_image_ = fill.get();
    addChild(std::move(fill));

    auto label = create_scope<UILabel>(context.getTextRenderer(), "");
    label->setOrderIndex(2);
    label->setAnchor({0, 0}, {1, 1});
    label->setVisible(false);
    label_ = label.get();
    addChild(std::move(label));
}

void UIProgressBar::setValue(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    if (value_ != value) {
        value_ = value;
        updateFillVisual();
    }
}

void UIProgressBar::setBackground(const Image& image) {
    if (background_image_) {
        background_image_->setImage(image);
    }
}

void UIProgressBar::setFill(const Image& image) {
    if (fill_image_) {
        fill_image_->setImage(image);
        updateFillVisual();
    }
}

void UIProgressBar::showLabel(bool show) {
    if (label_) {
        label_->setVisible(show);
    }
}

void UIProgressBar::setLabelText(std::string_view text) {
    if (label_) {
        label_->setText(std::string(text));
        invalidateLayout();
    }
}

void UIProgressBar::updateFillVisual() {
    if (!fill_image_) return;

    switch (fill_type_) {
        case ProgressBarFillType::LeftToRight:
            fill_image_->setAnchor({0, 0}, {value_, 1});
            break;
        case ProgressBarFillType::RightToLeft:
            fill_image_->setAnchor({1.0f - value_, 0}, {1, 1});
            break;
        case ProgressBarFillType::BottomToTop:
            fill_image_->setAnchor({0, 1.0f - value_}, {1, 1});
            break;
        case ProgressBarFillType::TopToBottom:
            fill_image_->setAnchor({0, 0}, {1, value_});
            break;
        default:
            break;
    }
}

void UIProgressBar::onLayout() {
    updateFillVisual();

    if (label_ && label_->isVisible()) {
        Vector2f size = label_->getSize();
        Vector2f my_size = getSize();
        Vector2f pos = (my_size - size) * 0.5f;
        if (glm::distance(label_->getPosition(), pos) > 0.001f) {
            label_->setPosition(pos);
        }
    }
}

} // namespace dodoe


