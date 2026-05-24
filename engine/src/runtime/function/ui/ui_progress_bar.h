#pragma once
#include "dopch.h"
#include "ui_element.h"
#include "ui_image.h"
#include "ui_label.h"

namespace dodoe {

enum class ProgressBarFillType {
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom
};

class UIProgressBar : public UIElement {
    float value_{0.0f};
    ProgressBarFillType fill_type_{ProgressBarFillType::LeftToRight};

    UIImage* background_image_{nullptr};
    UIImage* fill_image_{nullptr};
    UILabel* label_{nullptr};

public:
    UIProgressBar(engine::core::Context& context,
                  Vector2f position = {0.0f, 0.0f},
                  Vector2f size = {0.0f, 0.0f});

    void setValue(float value);
    float getValue() const { return value_; }

    void setBackground(const engine::render::Image& image);
    void setFill(const engine::render::Image& image);

    void showLabel(bool show);
    void setLabelText(std::string_view text);

    void setFillType(ProgressBarFillType type) { fill_type_ = type; updateFillVisual(); }

protected:
    void onLayout() override;
    void updateFillVisual();

};

} // namespace dodoe
