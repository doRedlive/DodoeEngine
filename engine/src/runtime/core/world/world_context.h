//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_WORLD_CONTEXT_H
#define DODOE_WORLD_CONTEXT_H

#include "dopch.h"

namespace dodoe {

    class Renderer;

    struct WorldContextCreateInfo {
        Renderer& renderer;
    };

    class WorldContext {
    public:
        Renderer& renderer;

        WorldContext(Renderer& in_renderer);

        static Scope<WorldContext> create(WorldContextCreateInfo create_info);
        static void destroy(Scope<WorldContext>& system_context);
    };

} // dodoe

#endif//DODOE_WORLD_CONTEXT_H
