// do@Redlive

#pragma once

#include "dopch.h"

#include "render_settings.h"
#include "runtime/function/graphics/gfx_context.h"
#include "framework/descriptor_table_manager.h"
#include "framework/shared_render_service.h"
#include "framework/viewport_manager.h"
#include "render_pipeline/render_pipeline.h"
#include "render_view/render_view_family.h"
#include "render_scene/render_scene.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        Scope<GfxContext> m_gfx{nullptr};
        Scope<RenderScene> m_render_scene{nullptr};
        Scope<RenderViewFamily> m_view_family{nullptr};
        Scope<RenderPipeline> m_render_pipeline{nullptr};
        Scope<ViewportManager> m_viewport_manager{nullptr};
        Scope<TextureManager> m_texture_manager{nullptr};
        Scope<DescriptorTableManager> m_descriptor_table{nullptr};
        Scope<SharedRenderService> m_shared_render_service{nullptr};

        WindowManager* m_window_manager{nullptr};

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] ViewportManager* getViewportManager() const { return m_viewport_manager.get(); }
        [[nodiscard]] RenderPipeline* getRenderingPipeline() const { return m_render_pipeline.get(); }
        [[nodiscard]] RenderScene* getRenderScene() const { return m_render_scene.get(); }
        [[nodiscard]] RenderViewFamily* getViewFamily() const { return m_view_family.get(); }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager.get(); }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service.get(); }

    private:
        bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();
    };

} // dodoe
