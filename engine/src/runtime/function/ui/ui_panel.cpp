#include "ui_panel.h"
#include "ui_imgui_utils.h"


namespace dodoe {

UIPanel::UIPanel(Vector2f position, Vector2f size,
                 std::optional<Color> background_color,
                 std::optional<Image> skin_image,
                 std::optional<NineSliceMargins> skin_margins)
    : UIElement(std::move(position), std::move(size)),
      background_color_(std::move(background_color)),
      skin_image_(std::move(skin_image))
{
    DO_TRACE("UIPanel created.");
    if (skin_image_ && skin_margins) {
        skin_image_->setNineSliceMargins(std::move(skin_margins));
    }
}

void UIPanel::setSkinImage(Image image) {
    skin_image_ = std::move(image);
}

void UIPanel::clearSkinImage() {
    skin_image_.reset();
}

void UIPanel::setNineSliceMargins(std::optional<NineSliceMargins> margins) {
    if (skin_image_) {
        skin_image_->setNineSliceMargins(std::move(margins));
    }
}

void UIPanel::renderSelf(Context& context) {
    (void)context;
    if (background_color_) {
        ui::drawFilledRect(getBounds(), *background_color_);
    }

    if (skin_image_) {
        const auto size = getLayoutSize();
        if (size.x > 0.0f && size.y > 0.0f) {
            ui::drawImage(*skin_image_, getScreenPosition(), size);
        }
    }
}

bool UIPanel::hasNineSliceSkin() const {
    return skin_image_.has_value() && skin_image_->hasNineSlice();
}

const Image* UIPanel::getSkinImage() const {
    return skin_image_ ? &*skin_image_ : nullptr;
}

const NineSliceMargins* UIPanel::getNineSliceMargins() const {
    if (skin_image_ && skin_image_->getNineSliceMargins()) {
        return &*skin_image_->getNineSliceMargins();
    }
    return nullptr;
}

void UIPanel::markNineSliceDirty() {
    if (skin_image_) {
        skin_image_->markNineSliceDirty();
    }
}

} // namespace dodoe 


