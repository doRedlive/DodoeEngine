#pragma once

#include "dopch.h"
#include "ui_interactive.h"

namespace dodoe {

class UIInputBlocker final : public UIInteractive {
public:
    UIInputBlocker(engine::core::Context& context, Vector2f position = {0.0f, 0.0f}, Vector2f size = {0.0f, 0.0f});

protected:
    void renderSelf(engine::core::Context& /*context*/) override {}
};

} // namespace dodoe
