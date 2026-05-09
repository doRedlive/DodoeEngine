//
// Created by Redlive on 2026/3/25.
//

#pragma once

#include "dopch.h"

#include "renderer_2d.h"
#include "interface/rhi.h"
#include "render_scene.h"

namespace dodoe {

    class RenderResource {
        rhi::DeviceHandle device_{};
        RenderScene render_scene_{};
        rhi::TextureHandle skybox_texture_{};
        mutable std::mutex submit_mutex_{};
        bool logic_main_camera_dirty_{false};
        Matrix4f logic_main_camera_view_proj_{1.0f};
        Vector3f logic_main_camera_position_{0.0f};
    public:
        void initilize(rhi::DeviceHandle device);
        void shutdown();

        void submitMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);

        [[nodiscard]] const RenderScene& renderScene() const;
        [[nodiscard]] RenderScene& getRenderScene();
        void rebuildSkyboxTexture();
        [[nodiscard]] rhi::TextureHandle getSkyboxTexture() const;

        void swapLogicRenderContext();

    private:
        void createSkyboxTextureInternal();
    };

    extern RenderResource* g_RenderResource;

} // dodoe
