// do@Redlive

#pragma once

#include "dopch.h"

#include "render_command.h"
#include "render_settings.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_frame/render_frame_scheduler.h"
#include "render_service/shared_render_service.h"
#include "render_view/render_view_manager.h"
#include "render_pipeline/render_pipeline.h"
#include "render_scene/render_scene.h"

#include "runtime/function/window/window_manager.h"
#include "runtime/core/container/mpmc_queue.h"
#include "runtime/core/thread/render_thread.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        static constexpr Size_t kGameCommandQueueCapacity = 256;

        Scope<GfxContext> m_gfx{nullptr};
        Scope<RenderFrameScheduler> m_frame_scheduler{nullptr};
        Scope<RenderScene> m_render_scene{nullptr};
        Scope<RenderPipeline> m_render_pipeline{nullptr};
        Scope<RenderViewManager> m_view_manager{nullptr};
        Scope<SharedRenderService> m_shared_render_service{nullptr};
        Scope<RenderThread> m_render_thread{nullptr};
        Bool m_context_acquired{false};

        WindowManager* m_window_manager{nullptr};

        MpmcQueue<ResourceCommand, kGameCommandQueueCapacity> m_resource_command_queue;
        MpmcQueue<SceneCommand, kGameCommandQueueCapacity> m_scene_command_queue;

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] RenderViewManager* getViewManager() const { return m_view_manager.get(); }
        [[nodiscard]] RenderPipeline* getRenderingPipeline() const { return m_render_pipeline.get(); }
        [[nodiscard]] RenderScene* getRenderScene() const { return m_render_scene.get(); }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service.get(); }

        void enqueueResourceCommand(ResourceCommand&& cmd);
        void enqueueSceneCommand(SceneCommand&& cmd);
        void realizeResourceCommand(ResourceCommand& cmd);
        void applySceneCommand(RenderScene& scene, SceneCommand& cmd);
        [[nodiscard]] Bool beginMainThreadFrame();
        void submitFrame();

    private:
        Bool initialize(const RenderSystemCreateInfo& info);
        void shutdown();
        void setupRenderThreading();

        [[nodiscard]] Bool acquireApplicationGraphicsContext();
        void releaseApplicationGraphicsContext();
        void renderFrameOnRenderThread();
        void renderFrame();
    };

} // namespace dodoe
