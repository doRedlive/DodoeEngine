#pragma once

// do@Redlive

#include "dopch.h"
#include "ui_element.h"
#include "runtime/core/utils/util.h"
#include "engine/render/image.h"

namespace dodoe {

class UIImage final : public UIElement {
private:
    engine::render::Image image_;

public:
    UIImage(std::string_view texture_path,
            Vector2f position = {0.0f, 0.0f},
            Vector2f size = {0.0f, 0.0f},
            Rect source_rect = {},
            bool is_flipped = false);

    UIImage(identifier texture_id,
            Vector2f position = {0.0f, 0.0f},
            Vector2f size = {0.0f, 0.0f},
            Rect source_rect = {},
            bool is_flipped = false);

    UIImage(engine::render::Image image,
            Vector2f position = {0.0f, 0.0f},
            Vector2f size = {0.0f, 0.0f});

    using UIElement::render;

    const engine::render::Image& getImage() const { return image_; }
    void setImage(engine::render::Image image) { image_ = std::move(image); }

    std::string_view getTexturePath() const { return image_.getTexturePath(); }
    identifier getTextureId() const { return image_.getTextureId(); }
    void setTexture(std::string_view texture_path) { image_.setTexture(texture_path); }

    const Rect& getSourceRect() const { return image_.getSourceRect(); }
    void setSourceRect(const Rect& source_rect) { image_.setSourceRect(source_rect); }

    bool isFlipped() const { return image_.isFlipped(); }
    void setFlipped(bool flipped) { image_.setFlipped(flipped); }
protected:
    void renderSelf(engine::core::Context& context) override;
};

} // namespace dodoe
