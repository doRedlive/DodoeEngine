// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class AnimatorSystem : public System {
    public:
        ~AnimatorSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe
