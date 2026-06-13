#include "runtime_frame_context_system.h"

#include "render_system_bridge.h"

namespace dodoe {

    RuntimeFrameContextSystem::~RuntimeFrameContextSystem() = default;

    void RuntimeFrameContextSystem::update(Registry& reg, float dt) {
        (void)reg;
        (void)dt;

        auto* camera = TryGetMainCamera();
        if (!camera) {
            return;
        }

        SubmitMainCameraViewProjection(camera->getViewProjectionMatrix(), camera->getPosition());
    }

} // dodoe
