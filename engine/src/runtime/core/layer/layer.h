// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {
    class DODOE_API Layer {
        String m_name;
    public:
        explicit Layer(String name);

        virtual ~Layer() = default;

        virtual void attach() = 0;
        virtual void detach() = 0;
        virtual void updateTick(float delta_time) = 0;
        virtual void renderTick() = 0;

        [[nodiscard]]
        const String& getName() const { return m_name; }
    };

} // dodoe
