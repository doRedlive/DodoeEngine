//
// Created by Redlive on 2026/3/25.
//

#pragma once

#include "dopch.h"

#include "renderer_2d.h"
#include "interface/rhi.h"
#include "render_scene.h"

namespace dodoe {

    struct MainCameraMeshSubmitData {
        identifier entity_id{0};
        identifier model_id{0};
        Matrix4f model_matrix{1.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
    };

    class RenderResource {
        rhi::DeviceHandle device_{};
        RenderScene render_scene_{};
        std::mutex submit_mutex_{};
    public:
        void initilize(rhi::DeviceHandle device);
        void shutdown();

        void submitMainCamera(const MainCameraMeshSubmitData& context);
        [[nodiscard]] const std::vector<MainCameraDrawPacket>& mainCameraPackets() const;
        void swapLogicRenderContext();
    };

    extern RenderResource* g_RenderResource;

} // dodoe