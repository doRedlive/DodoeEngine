#pragma once
#include "dopch.h"
#include "ui_element.h"
#include "ui_label.h"

namespace engine::core {
    class Context;
}

namespace dodoe {

class UIDragPreview final : public UIElement {
private:
    engine::core::Context& context_;
    engine::render::Image image_{};
    UILabel* count_label_{nullptr};
    float alpha_{0.6f};

public:
    UIDragPreview(engine::core::Context& context,
                  std::string_view font_path,
                  int font_size = 16,
                  Vector2f size = {0.0f, 0.0f});

    void setContent(const engine::render::Image& image, int count, Vector2f slot_size);
    void setAlpha(float alpha);
    void setFontPath(std::string_view font_path);

private:
    void renderSelf(engine::core::Context& context) override;
    void onLayout() override;
};

} // namespace dodoe
