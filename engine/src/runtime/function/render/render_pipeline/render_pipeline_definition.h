// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_settings.h"

namespace dodoe {

    struct RenderPipelineDefinition {
        String pipeline_type{};
        DynamicArray<String> feature_names{};
        CullingPath culling_path{CullingPath::CpuOnly};
    };

} // namespace dodoe
