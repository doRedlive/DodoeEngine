#include "../ui_interactive.h"
#include "ui_hover_state.h"
#include "ui_normal_state.h"
#include "ui_pressed_state.h"
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace dodoe {

void UIHoverState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_HOVER_ID);
    owner_->hover_enter();
    DO_TRACE("Switched to hover state.");
}

void UIHoverState::onMouseExit()
{
    owner_->hover_leave();
    owner_->setNextState(create_scope<UINormalState>(owner_));
}

void UIHoverState::onMousePressed()
{
    owner_->setNextState(create_scope<UIPressedState>(owner_));
}

} // namespace dodoe

