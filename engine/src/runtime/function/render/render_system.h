// do@Redlive : love u forever. owo

#pragma once

#include "dopch.h"

#include "render_graph.h"
#include "render_settings.h"
#include "interface/rhi_context.h"
#include "framework/camera.h"
#include "framework/descriptor_table_manager.h"
#include "framework/viewport_manager.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        Scope<Camera> m_camera{nullptr};
        Scope<RhiContext> m_rhi{nullptr};
        Scope<RenderGraph> m_render_graph{nullptr};
        Scope<ViewportManager> m_viewport_manager{nullptr};
        Scope<TextureManager> m_texture_manager{nullptr};
        Scope<DescriptorTableManager> m_descriptor_table{nullptr};

        WindowManager* m_window_manager{nullptr};
        UISystem* m_ui_system{nullptr};

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        void prepare();
        void present();

        [[nodiscard]] Camera& getMainCamera() { return *m_camera.get(); }
        [[nodiscard]] RhiContext* getRhi() const { return m_rhi.get(); }
        [[nodiscard]] ViewportManager* getViewportManager() const { return m_viewport_manager.get(); }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager.get(); }

    private:
        bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();

        void swapLogicRenderContext();
        void buildRenderGraph();
        void buildDeferredPipeline();
        void buildOnly2DPipeline();
    };

} // dodoe
