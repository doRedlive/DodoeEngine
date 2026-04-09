//
// Created by Redlive on 2026/3/25.
//

#include "render_resource.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

    RenderResource* g_RenderResource = new RenderResource();

    void RenderResource::initilize(rhi::DeviceHandle device) {
        device_ = device;
        cmd_list_ = device_->createCommandList();
    }

    void RenderResource::shutdown() {

    }

    void RenderResource::submit(const MeshSubmitData& context) {

    }

    void RenderResource::swapLogicRenderContext() {

    }

} // dodoe
