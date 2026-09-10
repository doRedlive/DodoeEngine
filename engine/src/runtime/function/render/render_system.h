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
#include "runtime/core/container/spsc_queue.h"
#include "runtime/core/thread/render_thread.h"

#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
#include "runtime/function/ui/imgui/imgui_builder.h"
#endif

#include <mutex>

namespace dodoe {

    class RenderViewFamily;

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    struct FrameCommandTargetView {
        RenderViewTarget* target{nullptr};
        Matrix4f view{1.0f};
        Matrix4f proj{1.0f};
        Bool show_editor{false};
    };

    struct FrameCommand {
        Float frame_time{0.0f};
        Float frame_delta{0.0f};
        Vector2i window_size{1, 1};
        Vector2i pixel_size{1, 1};
        DynamicArray<FrameCommandTargetView> targets{};
    };

    struct RenderFramePacket {
        FrameCommand frame{};
        DynamicArray<ResourceCommand> resource_commands{};
        DynamicArray<SceneCommand> scene_commands{};
#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
        ImGuiRenderPacket imgui{};
        DynamicArray<ImGuiViewportPacket> viewport_packets{};
#endif
    };

    class RenderSystem : public Managed<RenderSystem, RenderSystemCreateInfo> {
        static constexpr Size_t kFramePacketQueueCapacity = 4;

        Scope<GfxContext> m_gfx{nullptr};
        Scope<RenderFrameScheduler> m_frame_scheduler{nullptr};
        Scope<RenderScene> m_render_scene{nullptr};
        Scope<RenderPipeline> m_render_pipeline{nullptr};
        Scope<RenderViewManager> m_view_manager{nullptr};
        Scope<SharedRenderService> m_shared_render_service{nullptr};
        Scope<RenderThread> m_render_thread{nullptr};
        Bool m_context_acquired{false};

        WindowManager* m_window_manager{nullptr};

        std::function<void(GfxContext&, UInt32, RenderViewFamily&, RenderScene&)> m_baseline_renderer_hook{nullptr};

        SpscQueue<RenderFramePacket, kFramePacketQueueCapacity> m_frame_packet_queue;
        RenderFramePacket m_recording_packet{};
        std::mutex m_record_mutex{};
        FrameCommand m_last_frame_command{};

        friend class Managed<RenderSystem, RenderSystemCreateInfo>;
    public:
        [[nodiscard]] GfxContext* getGfx() const { return m_gfx.get(); }
        [[nodiscard]] RenderViewManager* getViewManager() const { return m_view_manager.get(); }
        [[nodiscard]] RenderPipeline* getRenderingPipeline() const { return m_render_pipeline.get(); }
        [[nodiscard]] RenderScene* getRenderScene() const { return m_render_scene.get(); }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return m_shared_render_service.get(); }
        [[nodiscard]] RenderFrameScheduler* getFrameScheduler() const { return m_frame_scheduler.get(); }

        void enqueueResourceCommand(ResourceCommand&& cmd);
        void enqueueSceneCommand(SceneCommand&& cmd);
        void stopRenderThread();
        void setBaselineRendererHook(std::function<void(GfxContext&, UInt32, RenderViewFamily&, RenderScene&)> hook) { m_baseline_renderer_hook = std::move(hook); }
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
