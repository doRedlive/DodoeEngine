#pragma once
#include "ui_state.h"

namespace dodoe {

class UINormalState final: public UIState {
    friend class dodoe::UIInteractive;
public:
    explicit UINormalState(dodoe::UIInteractive* owner) : UIState(owner) {}
    ~UINormalState() override = default;

private:
    void enter() override;
    void update(float, Context&) override {}

    void onMouseEnter() override;
};

} // namespace dodoe
