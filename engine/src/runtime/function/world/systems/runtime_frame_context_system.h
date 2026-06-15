#pragma once

#include "dopch.h"

#include "system.h"

namespace dodoe {

    class RuntimeFrameContextSystem final : public System {
    public:
        ~RuntimeFrameContextSystem() override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe
