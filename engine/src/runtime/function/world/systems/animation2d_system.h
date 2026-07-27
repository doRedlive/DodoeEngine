#ifndef DODOE_ANIMATION2D_SYSTEM_H
#define DODOE_ANIMATION2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class Animation2dSystem : public System {
    public:
        ~Animation2dSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe

#endif//DODOE_ANIMATION2D_SYSTEM_H
