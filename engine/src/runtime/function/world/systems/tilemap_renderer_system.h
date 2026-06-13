// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/world/systems/system.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/renderer_2d.h"

namespace dodoe {

    class TilemapRendererSystem : public System {
    public:
        ~TilemapRendererSystem() override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe

