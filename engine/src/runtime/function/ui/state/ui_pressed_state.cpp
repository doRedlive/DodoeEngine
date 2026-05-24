#include "../ui_interactive.h"
#include "ui_pressed_state.h"
#include "ui_normal_state.h"
#include "ui_hover_state.h"
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace dodoe {

void UIPressedState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_PRESSED_ID);
    owner_->playSoundEvent(UI_SOUND_EVENT_CLICK_ID);
    DO_TRACE("Switched to pressed state.");
}

void UIPressedState::onMouseReleased(bool is_inside)
{
    if (is_inside) {
        owner_->setNextState(create_scope<UIHoverState>(owner_));
        owner_->clicked();
    } else {
        owner_->setNextState(create_scope<UINormalState>(owner_));
    }
}

} // namespace dodoe
