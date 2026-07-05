// do@Redlive

#pragma once

#include "dopch.h"

#include "render_command.h"
#include "render_settings.h"
#include "runtime/function/graphics/gfx_context.h"
#include "framework/descriptor_table_manager.h"
#include "framework/shared_render_service.h"
#include "render_view/render_viewport.h"
#include "render_pipeline/render_pipeline.h"
#include "render_scene/render_scene.h"

#include "runtime/function/window/window_manager.h"
#include "runtime/core/thread/spsc_queue.h"

namespace dodoe {

    class RenderThread;
    class DrawThread;

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        static constexpr Size_t kGameCommandQueueCapacity = 256;

        Scope<GfxContext> m_gfx{nullptr};
        Scope<RenderScene> m_render_scene{nullptr};
        Scope<RenderPipeline> m_render_pipeline{nullptr};
        DynamicArray<Scope<RenderViewport>> m_render_viewports{};
        Scope<TextureManager> m_texture_manager{nullptr};
        Scope<DescriptorTableManager> m_descriptor_table{nullptr};
        Scope<SharedRenderService> m_shared_render_service{nullptr};
        RenderThread* m_render_thread{nullptr};

        WindowManager* m_window_manager{nullptr};

        SpscQueue<RenderCommand, kGameCommandQueueCapacity> m_game_command_queue;

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] RenderViewport* getMainRenderViewport() const { return m_render_viewports.empty() ? nullptr : m_render_viewports[0].get(); }
        [[nodiscard]] const DynamicArray<Scope<RenderViewport>>& getRenderViewports() const { return m_render_viewports; }
        [[nodiscard]] RenderPipeline* getRenderingPipeline() const { return m_render_pipeline.get(); }
        [[nodiscard]] RenderScene* getRenderScene() const { return m_render_scene.get(); }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager.get(); }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service.get(); }
        [[nodiscard]] RenderThread& getRenderThread() const { return *m_render_thread; }
        void setRenderThread(RenderThread* rt) { m_render_thread = rt; }

        void enqueueRenderCommand(RenderCommand&& cmd);
        void renderFrame(ThreadingMode mode, DrawThread* draw_thread);

    private:
        bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();
        void applyRenderCommand(RenderScene& scene, RenderCommand& cmd);
    };

} // dodoe
