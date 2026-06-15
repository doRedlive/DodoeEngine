#pragma once

#include "dopch.h"

#include "render_settings.h"
#include "runtime/function/graphics/gfx_context.h"
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

        WindowManager* m_window_manager{nullptr};

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] ViewportManager* getViewportManager() const { return m_viewport_manager.get(); }
        [[nodiscard]] RenderingPipeline* getRenderingPipeline() const { return m_rendering_pipeline.get(); }

    private:
        bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();
    };

} // dodoe
