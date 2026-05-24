#pragma once
#include "ui_state.h"

namespace dodoe {

class UIHoverState final: public UIState {
    friend class dodoe::UIInteractive;
public:
    explicit UIHoverState(dodoe::UIInteractive* owner) : UIState(owner) {}

    bool isHovered() const override { return true; }

private:
    void enter() override;
    void update(float, engine::core::Context&) override {}
    
    void onMouseExit() override;
    void onMousePressed() override;
};

} // namespace dodoe