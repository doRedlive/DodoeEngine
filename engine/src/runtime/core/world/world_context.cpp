//
// Created by Redlive on 2026/3/20.
//

#include "world_context.h"

#include "runtime/function/render/renderer.h"

namespace dodoe {

    WorldContext::WorldContext(Renderer& in_renderer) :
        renderer(in_renderer) {
    }

    Scope<WorldContext> WorldContext::create(WorldContextCreateInfo create_info) {
        auto context = Scope<WorldContext>(new WorldContext(create_info.renderer));
        return context;
    }

    void WorldContext::destroy(Scope<WorldContext>& system_context) {
        system_context.reset();
    }

} // dodoe
