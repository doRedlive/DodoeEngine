// do@Redlive
#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class CameraSystem : public System {
    public:
        ~CameraSystem() override;
        void update(Registry& reg, float dt) override;
    };

} // dodoe
