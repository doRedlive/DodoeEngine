//
// Created by Redlive on 2026/3/25.
//

#include "render_resource.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

    RenderResource* g_RenderResource = new RenderResource();

    void RenderResource::initilize(rhi::DeviceHandle device) {
        device_ = device;
    }

    void RenderResource::shutdown() {
        std::scoped_lock lock(submit_mutex_);
        render_scene_ = RenderScene{};
        device_ = nullptr;
    }

    void RenderResource::submitMainCamera(const MainCameraMeshSubmitData& context) {
        if (context.model_id == 0) {
            return;
        }

        MainCameraDrawPacket packet{};
        packet.entity_id = context.entity_id;
        packet.model_id = context.model_id;
        packet.model_matrix = context.model_matrix;
        packet.color = context.color;

        std::scoped_lock lock(submit_mutex_);
        render_scene_.submitMainCameraPacket(packet);
    }

    void RenderResource::swapLogicRenderContext() {
        std::scoped_lock lock(submit_mutex_);
        render_scene_.swapLogicRenderContext();
    }

    const std::vector<MainCameraDrawPacket>& RenderResource::mainCameraPackets() const {
        return render_scene_.mainCameraPackets();
    }

} // dodoe
