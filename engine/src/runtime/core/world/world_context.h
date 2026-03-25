//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_WORLD_CONTEXT_H
#define DODOE_WORLD_CONTEXT_H

#include "dopch.h"

namespace dodoe {

    class Renderer;
    class Camera;
    class PhysicsSystem;

    struct WorldContextCreateInfo {
        Renderer& renderer;
        Camera& camera;
        PhysicsSystem& physics_system;
    };

    class WorldContext {
    public:
        WorldContext(Renderer& in_renderer, Camera& in_camera, PhysicsSystem& in_physics_system);

        static Scope<WorldContext> create(const WorldContextCreateInfo& create_info);
        static void destroy(Scope<WorldContext>& system_context);

        [[nodiscard]] Renderer& renderer();
        [[nodiscard]] const Renderer& renderer() const;
        [[nodiscard]] Camera& camera();
        [[nodiscard]] const Camera& camera() const;
        [[nodiscard]] PhysicsSystem& physics_system();
        [[nodiscard]] const PhysicsSystem& physics_system() const;

    private:
        Renderer& renderer_;
        Camera& camera_;
        PhysicsSystem& physics_system_;
    };

} // dodoe

#endif//DODOE_WORLD_CONTEXT_H
