#pragma once

#include "dopch.h"

namespace dodoe {

    class GfxContext;
    class ShaderLibrary;
    class PipelineStateCache;
    class FullscreenPassSharedState;
    class GBufferMeshProcessor;
    class DirectionalShadowMeshProcessor;

    struct RenderingPassContext {
        GfxContext* gfx_context{nullptr};
        const ShaderLibrary* shader_library{nullptr};
        PipelineStateCache* pipeline_state_cache{nullptr};
        const FullscreenPassSharedState* fullscreen_pass_shared_state{nullptr};
        const GBufferMeshProcessor* gbuffer_mesh_processor{nullptr};
        const DirectionalShadowMeshProcessor* directional_shadow_mesh_processor{nullptr};

        [[nodiscard]] Bool isValid() const {
            return gfx_context != nullptr &&
                shader_library != nullptr &&
                pipeline_state_cache != nullptr &&
                fullscreen_pass_shared_state != nullptr &&
                gbuffer_mesh_processor != nullptr &&
                directional_shadow_mesh_processor != nullptr;
        }
    };

} // dodoe
