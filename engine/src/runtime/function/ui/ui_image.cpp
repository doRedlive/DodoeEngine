#include "ui_image.h"
#include "ui_imgui_utils.h"

#include <entt/core/hashed_string.hpp>

namespace dodoe {

UIImage::UIImage(std::string_view texture_path,
                 Vector2f position,
                 Vector2f size,
                 Rect source_rect,
                 bool is_flipped)
    : UIElement(std::move(position), std::move(size)),
      image_(texture_path, source_rect, is_flipped)
{
    if (image_.getTextureId() == entt::null) {
        DO_WARN("UIImage created with empty texture id (by texture_path).");
    }
    DO_TRACE("UIImage created.");
}

UIImage::UIImage(identifier texture_id,
                 Vector2f position,
                 Vector2f size,
                Rect source_rect,
                 bool is_flipped)
    : UIElement(std::move(position), std::move(size)),
      image_(texture_id, source_rect, is_flipped)
{
    if (image_.getTextureId() == entt::null) {
        DO_WARN("UIImage created with empty texture id (by texture_id).");
    }
    DO_TRACE("UIImage created.");
}

UIImage::UIImage(Image image,
                 Vector2f position,
                 Vector2f size)
    : UIElement(std::move(position), std::move(size)),
      image_(std::move(image))
{
    DO_TRACE("UIImage created.");
}

void UIImage::renderSelf(Context& context) {
    if (image_.getTextureId() == entt::null) {
        return;
    }

    auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        DO_WARN("UIImage has invalid size ({}, {}).", size.x, size.y);
        return;
    }

    ui::drawImage(image_, getScreenPosition(), size);
}

} // namespace dodoe 


