#pragma once

// do@Redlive

#include "dopch.h"
#include "ui_element.h"
#include "runtime/core/utils/util.h"
#include "engine/render/image.h"
#include "engine/render/nine_slice.h"

namespace dodoe {

class UIPanel final : public UIElement {
    std::optional<Color> background_color_;
    std::optional<engine::render::Image> skin_image_;

public:
    UIPanel(Vector2f position = {0.0f, 0.0f},
            Vector2f size = {0.0f, 0.0f},
            std::optional<Color> background_color = std::nullopt,
            std::optional<engine::render::Image> skin_image = std::nullopt,
            std::optional<engine::render::NineSliceMargins> skin_margins = std::nullopt);

public:
    void setBackgroundColor(std::optional<Color> background_color) { background_color_ = std::move(background_color); }
    [[nodiscard]] const std::optional<Color>& getBackgroundColor() const { return background_color_; }

    void setSkinImage(engine::render::Image image);
    void clearSkinImage();
    [[nodiscard]] bool hasNineSliceSkin() const;
    [[nodiscard]] const engine::render::Image* getSkinImage() const;
    [[nodiscard]] const engine::render::NineSliceMargins* getNineSliceMargins() const;
    void setNineSliceMargins(std::optional<engine::render::NineSliceMargins> margins);
    void markNineSliceDirty();

protected:
    void renderSelf(engine::core::Context& context) override;
};

} // namespace dodoe
