//
// Created by Redlive on 2026/3/24.
//

#ifndef DODOE_CAMERA2D_SYSTEM_H
#define DODOE_CAMERA2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

#include "runtime/core/utils/tags.h"

namespace dodoe {

    class Camera2dSystem : public System {
    public:
        ~Camera2dSystem() override;

        void update(Registry& reg, float dt) override;
    };

} // dodoe

#endif//DODOE_CAMERA2D_SYSTEM_H
