//
// Created by Redlive on 2026/3/20.
//

#include "world_context.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/camera/camera.h"
#include "runtime/function/physics/physics_system.h"

namespace dodoe {

    WorldContext::WorldContext(Renderer& in_renderer, Camera& in_camera, PhysicsSystem& in_physics_system) :
        renderer_(in_renderer), camera_(in_camera), physics_system_(in_physics_system) {
    }

    Scope<WorldContext> WorldContext::create(const WorldContextCreateInfo& create_info) {
        auto context = Scope<WorldContext>(new WorldContext(create_info.renderer, create_info.camera, create_info.physics_system));
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

    PhysicsSystem& WorldContext::physics_system() {
        return physics_system_;
    }

    const PhysicsSystem& WorldContext::physics_system() const {
        return physics_system_;
    }

} // dodoe
