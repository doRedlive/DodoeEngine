//
// Created by Redlive on 2026/3/20.
//

#include "world_context.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/camera/camera.h"

namespace dodoe {

    WorldContext::WorldContext(Renderer& in_renderer, Camera& in_camera) :
        renderer_(in_renderer), camera_(in_camera) {
    }

    Scope<WorldContext> WorldContext::create(const WorldContextCreateInfo& create_info) {
        auto context = Scope<WorldContext>(new WorldContext(create_info.renderer, create_info.camera));
        return context;
    }

    void WorldContext::destroy(Scope<WorldContext>& system_context) {
        system_context.reset();
    }

    Renderer& WorldContext::renderer() {
        return renderer_;
    }

    const Renderer& WorldContext::renderer() const {
        return renderer_;
    }

    Camera& WorldContext::camera() {
        return camera_;
    }

    const Camera& WorldContext::camera() const {
        return camera_;
    }

} // dodoe
