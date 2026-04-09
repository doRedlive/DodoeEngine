//
// Created by Redlive on 2026/3/25.
//

#pragma once

#include "dopch.h"

#include "renderer_2d.h"
#include "interface/rhi.h"

namespace dodoe {

    struct MeshSubmitData {
        identifier model_id{0};
    };

    class RenderResource {
        rhi::DeviceHandle device_{};
        rhi::CommandListHandle cmd_list_{};
    public:
        void initilize(rhi::DeviceHandle device);
        void shutdown();

        void submit(const MeshSubmitData& context);
        void swapLogicRenderContext();
    };

    extern RenderResource* g_RenderResource;

} // dodoe