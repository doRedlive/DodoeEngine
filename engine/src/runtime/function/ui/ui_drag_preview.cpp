#include "dopch.h"
// do@Redlive

#include "ui_drag_preview.h"
#include "ui_imgui_utils.h"
#include <algorithm>
#include <limits>
#include <memory>
#include "runtime/core/math/math.h"

namespace dodoe {
namespace {
constexpr float DEFAULT_ALPHA = 0.6f;
constexpr Vector2f COUNT_PADDING{2.0f, 2.0f};
} // namespace

UIDragPreview::UIDragPreview(Context& context,
                             std::string_view font_path,
                             int font_size,
                             Vector2f size)
    : UIElement({0.0f, 0.0f}, size),
      context_(context),
      alpha_(DEFAULT_ALPHA) {
    auto label = create_scope<UILabel>(context_.getTextRenderer(), "", font_path, font_size);
    label->setVisible(false);
    count_label_ = label.get();
    addChild(std::move(label));

    setAnchor({0.0f, 0.0f}, {0.0f, 0.0f});
    setPivot({0.5f, 0.5f});
    setOrderIndex((std::numeric_limits<int>::max)() / 2);
}

void UIDragPreview::setContent(const Image& image, int count, Vector2f slot_size) {
    image_ = image;
    setSize(slot_size);

    if (count_label_) {
        if (count > 1) {
            count_label_->setText(std::to_string(count));
            count_label_->setVisible(true);
        } else {
            count_label_->setVisible(false);
        }
    }
    invalidateLayout();
}

void UIDragPreview::setAlpha(float alpha) {
    alpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

void UIDragPreview::setFontPath(std::string_view font_path) {
    if (count_label_) {
        count_label_->setFontPath(font_path);
    }
}

void UIDragPreview::renderSelf(Context& context) {
    if (image_.getTextureId() == entt::null) {
        DO_WARN("UIDragPreview: image is invalid.");
        return;
    }
    const auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        DO_WARN("UIDragPreview: invalid size ({}, {}).", size.x, size.y);
        return;
    }

    ui::drawImage(image_, getScreenPosition(), size, Color{1.0f, 1.0f, 1.0f, alpha_});
}

void UIDragPreview::onLayout() {
    if (!count_label_ || !count_label_->isVisible()) {
        return;
    }

    const Vector2f lbl_size = count_label_->getSize();
    Vector2f pos = getSize();
    pos -= (lbl_size + COUNT_PADDING);

    if (Math::Distance(count_label_->getPosition(), pos) > 0.001f) {
        count_label_->setPosition(pos);
    }
}

} // namespace dodoe

