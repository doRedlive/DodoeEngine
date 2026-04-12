#ifndef DODOE_MODEL_RENDERER_SYSTEM_H
#define DODOE_MODEL_RENDERER_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class ModelRendererSystem : public System {
    public:
        ~ModelRendererSystem() override = default;

        void update(Registry& reg, float dt) override {
            (void)dt;
            auto view = reg.view<ModelRendererComponent>();
            for (auto entity : view) {
                (void)reg.get<ModelRendererComponent>(entity);
            }
        }
    };

} // dodoe

#endif//DODOE_MODEL_RENDERER_SYSTEM_H
