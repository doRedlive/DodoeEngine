// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/renderer_2d.h"

namespace dodoe {

    class SpriteRendererSystem : public System {
    public:
        ~SpriteRendererSystem() override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe
