#include "../ui_interactive.h"
#include "ui_normal_state.h"
#include "ui_hover_state.h"
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace dodoe {

void UINormalState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_NORMAL_ID);
    DO_TRACE("Switched to normal state.");
}

void UINormalState::onMouseEnter()
{
    owner_->playSoundEvent(UI_SOUND_EVENT_HOVER_ID);
    owner_->setNextState(create_scope<UIHoverState>(owner_));
}

} // namespace dodoe

