//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_WORLD_CONTEXT_H
#define DODOE_WORLD_CONTEXT_H

#include "dopch.h"

namespace dodoe {

    class Renderer;
    class Camera;

    struct WorldContextCreateInfo {
        Renderer& renderer;
        Camera& camera;
    };

    class WorldContext {
    public:
        WorldContext(Renderer& in_renderer, Camera& in_camera);

        static Scope<WorldContext> create(const WorldContextCreateInfo& create_info);
        static void destroy(Scope<WorldContext>& system_context);

        [[nodiscard]] Renderer& renderer();
        [[nodiscard]] const Renderer& renderer() const;
        [[nodiscard]] Camera& camera();
        [[nodiscard]] const Camera& camera() const;

    private:
        Renderer& renderer_;
        Camera& camera_;
    };

} // dodoe

#endif//DODOE_WORLD_CONTEXT_H
