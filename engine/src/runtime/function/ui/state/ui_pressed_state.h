#pragma once
#include "ui_state.h"

namespace dodoe {

class UIPressedState final: public UIState {
    friend class dodoe::UIInteractive;
public:
    explicit UIPressedState(dodoe::UIInteractive* owner) : UIState(owner) {}

    bool isHovered() const override { return true; }
    bool isPressed() const override { return true; }

private:
    void enter() override;
    void onMouseReleased(bool is_inside) override;
};

} // namespace dodoe
