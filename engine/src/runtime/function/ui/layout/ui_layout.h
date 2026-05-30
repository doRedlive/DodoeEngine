#pragma once
#include "../ui_element.h"

namespace dodoe {

class UILayout : public UIElement {
public:
    using UIElement::UIElement; // 使用基类构造函数

    ~UILayout() override = default;

    /**
     * @brief 强制更新布局
     * 
     * 通常只需要调用 ensureLayout()，但有时需要手动触发布局刷新。
     */
    void forceLayout() {
        invalidateLayout();
        ensureLayout();
    }
};

} // namespace dodoe

