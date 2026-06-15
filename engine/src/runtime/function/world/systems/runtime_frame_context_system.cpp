#include "runtime_frame_context_system.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/framework/camera.h"

namespace dodoe {

    RuntimeFrameContextSystem::~RuntimeFrameContextSystem() = default;

    void RuntimeFrameContextSystem::update(Registry& reg, float dt) {
        (void)reg;
        (void)dt;

        auto* camera = Renderer::GetMainCamera();
        if (!camera) {
            return;
        }

        Renderer::SetMainCameraViewProjection(camera->getViewProjectionMatrix(), camera->getPosition());
    }

} // dodoe
