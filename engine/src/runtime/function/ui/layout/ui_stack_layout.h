#pragma once
#include "ui_layout.h"

namespace dodoe {

enum class Orientation {
    Horizontal,
    Vertical
};

enum class Alignment {
    Start,
    Center,
    End
};

class UIStackLayout : public UILayout {
private:
    Orientation orientation_{Orientation::Vertical};
    float spacing_{0.0f};
    Alignment alignment_{Alignment::Start};
    bool auto_resize_{false};
    
public:
    UIStackLayout(Vector2f position = {0.0f, 0.0f}, Vector2f size = {0.0f, 0.0f});

    void setOrientation(Orientation orientation);
    void setSpacing(float spacing);
    void setContentAlignment(Alignment alignment);
    void setAutoResize(bool auto_resize) { auto_resize_ = auto_resize; invalidateLayout(); }

protected:
    void onLayout() override;
};

} // namespace dodoe

