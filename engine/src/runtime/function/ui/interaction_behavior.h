#pragma once

#include "dopch.h"

namespace dodoe {
    class UIInteractive;

    class InteractionBehavior {
    public:
        InteractionBehavior() = default;
        virtual ~InteractionBehavior() = default;

        InteractionBehavior(const InteractionBehavior&) = delete;
        InteractionBehavior& operator=(const InteractionBehavior&) = delete;
        InteractionBehavior(InteractionBehavior&&) = delete;
        InteractionBehavior& operator=(InteractionBehavior&&) = delete;

        virtual void onAttach(UIInteractive& /*owner*/) {}

        virtual void onHoverEnter(UIInteractive& /*owner*/) {}
        virtual void onHoverExit(UIInteractive& /*owner*/) {}
        virtual void onPressed(UIInteractive& /*owner*/) {}
        virtual void onReleased(UIInteractive& /*owner*/, bool /*inside*/) {}
        virtual void onClick(UIInteractive& /*owner*/) {}

        virtual void onDragBegin(UIInteractive& /*owner*/, const Vector2f& /*pos*/) {}
        virtual void onDragUpdate(UIInteractive& /*owner*/, const Vector2f& /*pos*/, const Vector2f& /*delta*/) {}
        virtual void onDragEnd(UIInteractive& /*owner*/, const Vector2f& /*pos*/, bool /*accepted*/) {}
    };

} // namespace dodoe
