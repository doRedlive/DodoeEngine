// do@Redlive : love u forever. owo

#pragma once

#include "dopch.h"

#include "render_settings.h"
#include "interface/gfx_context.h"
#include "framework/camera.h"
#include "framework/descriptor_table_manager.h"
#include "framework/viewport_manager.h"
#include "renderer.h"
#include "rendering_pipeline/rendering_pipeline.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        Scope<Camera> m_camera{nullptr};
        Scope<GfxContext> m_gfx{nullptr};
        Scope<RenderingPipeline> m_rendering_pipeline{nullptr};
        Scope<ViewportManager> m_viewport_manager{nullptr};
        Scope<TextureManager> m_texture_manager{nullptr};
        Scope<DescriptorTableManager> m_descriptor_table{nullptr};
        Scope<Renderer> m_renderer{nullptr};
        mutable std::mutex m_submit_mutex{};
        bool m_logic_main_camera_dirty_{false};
        Matrix4f m_logic_main_camera_view_proj_{1.0f};
        Vector3f m_logic_main_camera_position_{0.0f};

        WindowManager* m_window_manager{nullptr};
        UISystem* m_ui_system{nullptr};

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        void prepare();
        void present();
        void submitMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);

        [[nodiscard]] Renderer* getRenderer() const { return m_renderer.get(); }
        [[nodiscard]] gfx::TextureHandle getSkyboxTexture() const;

        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] ViewportManager* getViewportManager() const { return m_viewport_manager.get(); }

    private:
        bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();

        void swapLogicRenderContext();
    };

} // dodoe
