#pragma once

#include "dopch.h"

#include "system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    class RuntimeFrameContextSystem final : public System {
    public:
        ~RuntimeFrameContextSystem() override = default;

        void update(Registry& reg, float dt) override {
            (void)reg;
            (void)dt;

            auto* render_system = Application::Self().context().render_system.get();
            if (!render_system) {
                return;
            }

            const auto& camera = render_system->getMainCamera();
            g_RenderResource->submitMainCameraViewProjection(camera.getViewProjectionMatrix(), camera.getPosition());
        }
    };

} // dodoe
