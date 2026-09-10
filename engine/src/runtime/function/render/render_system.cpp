// do@Redlive
// Knight!

#include "render_system.h"

#include <chrono>

#include "render_settings.h"
#include "runtime/core/memory/memory.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/core/thread/render_thread.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"
#include "render_frame/frame_telemetry.h"
#ifdef DODOE_DEBUG_ENABLED
#include "runtime/function/ui/imgui/imgui_builder.h"
#include "runtime/function/ui/imgui/imgui_viewport_renderer.h"
#endif//DODOE_DEBUG_ENABLED

namespace dodoe {

    Bool RenderSystem::initialize(const RenderSystemCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::initialize", "startup");
        m_window_manager = info.window_manager;

        auto window = m_window_manager->getWindow();
        auto backend_api = RenderSettings::GetRenderBackendApiType();
        DO_INFO("Render Backend API is {}.", RenderSettings::GetRenderBackendApiTypeStr());
        m_view_manager = RenderViewManager::Create({m_window_manager});

        const bool enable_validation =
#ifdef DODOE_DEBUG_ENABLED
            true;
#else
            false;
#endif//DODOE_DEBUG_ENABLED
        Vector2i init_pixel = window->getPixelSize();
        if (window->isHostMode()) {
            init_pixel.x = std::max(init_pixel.x, 1);
            init_pixel.y = std::max(init_pixel.y, 1);
        }
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation,
            RenderSettings::GetFeatureSettings(), window->isHostMode() ? window->getNativeHandle() : nullptr,
            static_cast<UInt32>(init_pixel.x), static_cast<UInt32>(init_pixel.y)});
        GDrawCommandList.setDevice(*m_gfx);

        m_frame_scheduler = RenderFrameScheduler::Create({m_gfx->getDevice()});
        m_shared_render_service = SharedRenderService::Create({m_gfx.get()});
        m_render_scene = RenderScene::Create({m_shared_render_service.get()});
        m_render_pipeline = RenderPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_shared_render_service.get()
        });
        const Bool initialized = m_render_scene && m_shared_render_service && m_render_pipeline && m_frame_scheduler;
        DO_INFO("Initialization {}.", initialized ? "completed" : "failed");
        if (!initialized) return false;
        setupRenderThreading();
        return true;
    }

    void RenderSystem::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::shutdown", "shutdown");
        DO_PROFILE_MARK("RenderSystem::shutdown.stopRenderThread", "shutdown");
        if (m_render_thread) {
            m_render_thread->stop();
            m_render_thread.reset();
        }
        (void)acquireApplicationGraphicsContext();
        DO_PROFILE_MARK("RenderSystem::shutdown.releaseQueues", "shutdown");
        RenderFramePacket leftover_packet;
        while (m_frame_packet_queue.tryPop(leftover_packet)) {
        }
        m_frame_packet_queue.close();
        {
            std::lock_guard<std::mutex> lock(m_record_mutex);
            m_recording_packet.resource_commands.clear();
            m_recording_packet.scene_commands.clear();
        }
        m_gfx->waitForIdle();
        RenderPipeline::Destroy(m_render_pipeline);
        RenderFrameScheduler::Destroy(m_frame_scheduler);
        RenderScene::Destroy(m_render_scene);
        SharedRenderService::Destroy(m_shared_render_service);
        m_gfx->waitForIdle();
        m_gfx->clearGarbage();
        GfxContext::Destroy(m_gfx);
        RenderViewManager::Destroy(m_view_manager);
        DO_INFO("Shutdown completed.");
    }

    void RenderSystem::stopRenderThread() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::stopRenderThread", "shutdown");
        if (m_render_thread) {
            m_render_thread->stop();
        }
    }

    void RenderSystem::enqueueResourceCommand(ResourceCommand&& cmd) {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        m_recording_packet.resource_commands.push_back(std::move(cmd));
    }

    void RenderSystem::enqueueSceneCommand(SceneCommand&& cmd) {
        std::lock_guard<std::mutex> lock(m_record_mutex);
        m_recording_packet.scene_commands.push_back(std::move(cmd));
    }

    Bool RenderSystem::acquireApplicationGraphicsContext() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::acquireApplicationGraphicsContext", "synchronization");
        return m_gfx->acquireOpenGLContext();
    }

    void RenderSystem::releaseApplicationGraphicsContext() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::releaseApplicationGraphicsContext", "synchronization");
        m_gfx->releaseOpenGLContext();
    }

    Bool RenderSystem::beginMainThreadFrame() {
        return true;
    }

    void RenderSystem::submitFrame() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::submitFrame", "frame");
        if (!m_render_thread) return;
#ifdef DODOE_DEBUG_ENABLED
        if (!RenderSettings::IsEnableBaselineRender()) {
            ImGuiBuilder::RenderPlatformWindows();
        }
#endif//DODOE_DEBUG_ENABLED

        FrameCommand frame_command{};
        if (auto window = m_window_manager->getWindow()) {
            frame_command.window_size = Vector2i(window->getWidth(), window->getHeight());
            frame_command.pixel_size = window->getPixelSize();
        }
        if (auto* time_sys = GetTimeSystem()) {
            frame_command.frame_time = time_sys->getCurrentTime();
            frame_command.frame_delta = time_sys->getDeltaTime();
        }
        for (auto& target : m_view_manager->getTargets()) {
            auto* cam = target->getCamera();
            FrameCommandTargetView entry{};
            entry.target = target.get();
            entry.view = cam ? cam->getView() : Matrix4f(1.0f);
            entry.proj = cam ? cam->getProj() : Matrix4f(1.0f);
#ifdef DODOE_EDITOR_ENABLED
            entry.show_editor = cam && cam->isEditorCamera();
#endif//DODOE_EDITOR_ENABLED
            frame_command.targets.push_back(std::move(entry));
        }

        RenderFramePacket packet{};
        packet.frame = std::move(frame_command);
#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
        if (!RenderSettings::IsEnableBaselineRender()) {
            packet.imgui = ImGuiBuilder::TakeRenderPacket();
            packet.viewport_packets = ImGuiBuilder::TakeViewportPackets();
        }
#endif
        {
            std::lock_guard<std::mutex> lock(m_record_mutex);
            packet.resource_commands = std::move(m_recording_packet.resource_commands);
            packet.scene_commands = std::move(m_recording_packet.scene_commands);
            m_recording_packet.resource_commands.clear();
            m_recording_packet.scene_commands.clear();
        }
        m_frame_packet_queue.push(std::move(packet));

        if (RenderSettings::IsSingleThread()) {
            m_render_thread->executeFrameOnce();
        } else {
            m_render_thread->submitFrame();
        }
    }

    void RenderSystem::setupRenderThreading() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::setupRenderThreading", "startup");
        if (RenderSettings::IsSingleThread()) {
            m_render_thread = create_scope<RenderThread>(RenderFrameTask([this] {
                renderFrame();
            }));
            m_render_thread->start(false);
            DO_INFO("Single-thread rendering initialized.");
            return;
        }

        releaseApplicationGraphicsContext();
        m_render_thread = create_scope<RenderThread>(RenderFrameTask([this] {
            renderFrameOnRenderThread();
        }), RenderFrameTask([this] {
            releaseApplicationGraphicsContext();
            m_context_acquired = false;
        }));
        m_render_thread->start(true);
        DO_INFO("Dual-thread rendering initialized.");
    }

    void RenderSystem::renderFrameOnRenderThread() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::renderFrameOnRenderThread", "frame");
        if (!m_context_acquired) {
            if (!acquireApplicationGraphicsContext()) {
                DO_ERROR("RenderSystem failed to acquire graphics context for render thread.");
                return;
            }
            m_context_acquired = true;
        }
        renderFrame();
    }

    void RenderSystem::renderFrame() {
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::renderFrame", "frame");
        GfxRenderScope render_scope;
        Memory::ResetFrame();
        RenderFrameCounters::Self().reset();

        RenderFramePacket packet;
        if (!m_frame_packet_queue.tryPop(packet)) {
            packet.frame = m_last_frame_command;
        } else {
            m_last_frame_command = packet.frame;
        }
#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
        ImGuiBuilder::SetActiveRenderPacket(&packet.imgui);
#endif

        auto* gfx = m_gfx.get();
        auto* pipeline = m_render_pipeline.get();

        Vector2i cur_window = packet.frame.window_size;
        Vector2i cur_pixel  = packet.frame.pixel_size;

        if (cur_pixel.x <= 0 || cur_pixel.y <= 0) {
            return;
        }

        Bool any_window_dirty = false;
        for (auto& entry : packet.frame.targets) {
            auto* target = entry.target;
            const auto& vp = target->getViewport();
            if (vp.getWindowSize().x != cur_window.x || vp.getWindowSize().y != cur_window.y ||
                vp.getPixelSize().x != cur_pixel.x || vp.getPixelSize().y != cur_pixel.y) {
                target->resize(cur_window, cur_pixel);
                any_window_dirty = true;
            }
        }

        if (any_window_dirty) {
            DO_PROFILE_SCOPE_CATEGORY("RenderSystem::resizeTargets", "swapchain");
            m_gfx->waitForIdle();
            m_frame_scheduler->retireCompletedFrames();
            if (!m_gfx->recreateSwapchain(static_cast<UInt32>(cur_pixel.x),
                                          static_cast<UInt32>(cur_pixel.y))) {
                DO_ERROR("RenderSystem failed to recreate swapchain on window resize.");
            }
            pipeline->onResize(static_cast<UInt32>(cur_pixel.x),
                               static_cast<UInt32>(cur_pixel.y));
            m_gfx->clearGarbage();
        }

        for (auto& entry : packet.frame.targets) {
            entry.target->clearGeometryDirty();
        }

        auto* scene = m_render_scene.get();

        {
            DO_PROFILE_SCOPE_CATEGORY("RenderSystem::realizeResourceCommands", "render-command");
            for (auto& cmd : packet.resource_commands) {
                realizeResourceCommand(cmd);
            }
        }

        {
            DO_PROFILE_SCOPE_CATEGORY("RenderSystem::executeDeferredCommands", "render-command");
            auto pending = GDrawCommandList.detachRecordedCommands();
            if (!pending.isEmpty()) {
                auto& gfx_cmd = m_gfx->getCommandList();
                gfx_cmd->open();
                pending.execute(*gfx_cmd);
                gfx_cmd->close();
                m_gfx->getDevice()->executeCommandList(gfx_cmd);
            }
        }

        {
            DO_PROFILE_SCOPE_CATEGORY("RenderSystem::applySceneCommands", "render-command");
            for (auto& cmd : packet.scene_commands) {
                applySceneCommand(*scene, cmd);
            }
        }

        UInt32 image_index = 0;
        if (!m_gfx->acquireNextSwapchainImage(image_index)) {
            return;
        }
        DO_PROFILE_MARK("RenderSystem::renderFrame.swapchainAcquired", "swapchain");
#ifdef DODOE_PERF_ENABLED
        const auto render_frame_start = std::chrono::steady_clock::now();
#endif

        auto frame_ctx = m_frame_scheduler->beginFrame(image_index);
        m_shared_render_service->processDeferredDeletions(frame_ctx.frame_number);
        DO_PROFILE_SCOPE_CATEGORY("RenderSystem::buildFrame", "frame");

        const Float frame_time = packet.frame.frame_time;
        const Float frame_delta = packet.frame.frame_delta;

        frame_ctx.command_list->setDevice(m_gfx->getDevice());
        if (RenderSettings::IsEnableBaselineRender() && RenderSettings::IsGpuDrivenSupported()) {
            DrawCommandList upload_list;
            upload_list.setDevice(*m_gfx);
            scene->flushUpdates(upload_list);
            auto& upload_cmd = m_gfx->getCommandList();
            upload_cmd->open();
            upload_list.execute(upload_cmd);
            upload_cmd->close();
            m_gfx->getDevice()->executeCommandList(upload_cmd);
        } else {
            scene->flushUpdates(*frame_ctx.command_list);
        }
        m_shared_render_service->getMaterialSystem()->prepare();
        DO_PROFILE_MARK("RenderSystem::renderFrame.sceneFlushed", "frame");

        DO_PROFILE_MARK("RenderSystem::renderFrame.renderViewTargets", "frame");
        if (RenderSettings::IsEnableBaselineRender()) {
            static Bool s_baseline_branch_logged = false;
            if (!s_baseline_branch_logged) {
                s_baseline_branch_logged = true;
                DO_INFO("RenderSystem: baseline branch active, hook_valid={}",
                    static_cast<bool>(m_baseline_renderer_hook));
            }
            if (m_baseline_renderer_hook) {
                for (auto& entry : packet.frame.targets) {
                    auto* target = entry.target;
                    auto family = target->getViewport().buildViewFamily(*scene, frame_time, frame_delta, entry.view, entry.proj, false);
                    family.buildVisiblePrimitives(*scene);
                    m_baseline_renderer_hook(*gfx, frame_ctx.swapchain_image_index, family, *scene);
                }
            }
        } else {
            for (auto& entry : packet.frame.targets) {
                auto* target = entry.target;
                Bool show_editor = entry.show_editor;
                auto family = target->getViewport().buildViewFamily(*scene, frame_time, frame_delta, entry.view, entry.proj, show_editor);
                pipeline->render(
                    family, *scene, frame_ctx.swapchain_image_index, *frame_ctx.command_list,
                    frame_ctx.staging, frame_ctx.transient_resource_pool);
            }
        }

        {
            auto& gfx_cmd = m_gfx->getCommandList();
            gfx_cmd->open();
            frame_ctx.command_list->execute(gfx_cmd);
            gfx_cmd->close();
            m_gfx->getDevice()->executeCommandList(gfx_cmd);
        }
        gfx->getDevice()->setEventQuery(frame_ctx.completion_query, GfxCommandQueue::Graphics);
        DO_PROFILE_MARK("RenderSystem::renderFrame.commandsSubmitted", "frame");
        if (!m_gfx->presentSwapchainImage(frame_ctx.swapchain_image_index)) {
            DO_ERROR("RenderSystem failed to present swapchain image {}", frame_ctx.swapchain_image_index);
        }
        m_gfx->clearGarbage();

#ifdef DODOE_PERF_ENABLED
        {
            const auto render_frame_end = std::chrono::steady_clock::now();
            const auto& counters = RenderFrameCounters::Self();
            FrameTelemetry telemetry{};
            telemetry.frame_number = frame_ctx.frame_number;
            telemetry.delta_time_ms = frame_delta * 1000.0f;
            telemetry.render_thread_ms =
                std::chrono::duration<Float, std::milli>(render_frame_end - render_frame_start).count();
            telemetry.draw_call_count = static_cast<UInt32>(counters.getDrawCalls());
            telemetry.indirect_draw_call_count = static_cast<UInt32>(counters.getIndirectDrawCalls());
            telemetry.dispatch_count = static_cast<UInt32>(counters.getDispatches());
            telemetry.barrier_count = static_cast<UInt32>(counters.getBarriers());
            telemetry.drawn_instance_count = counters.getDrawnInstances();
            telemetry.pending_deletion_count = 0;
            m_frame_scheduler->recordTelemetry(telemetry);
        }
#endif

#ifdef DODOE_DEBUG_ENABLED
        if (!RenderSettings::IsEnableBaselineRender()) {
            for (auto& entry : packet.viewport_packets) {
                ImGuiViewportRenderer::RenderWindowOnRenderThread(entry.viewport, entry.packet);
            }
            m_gfx->acquireOpenGLContext();
        }
#endif//DODOE_DEBUG_ENABLED
    }

    void RenderSystem::realizeResourceCommand(ResourceCommand& cmd) {
        switch (cmd.type) {
        case ResourceCommandType::CreateTexture:
            if (auto* texture_manager = m_shared_render_service->getTextureManager()) {
                (void)texture_manager->realizeTexture(cmd);
            }
            break;
        case ResourceCommandType::CreateBuffer:
            if (cmd.buffer) {
                cmd.buffer->initializeGpu(m_gfx->getDevice());
                if (!cmd.resource_data.empty()) {
                    GDrawCommandList.writeBuffer(cmd.buffer, cmd.resource_data.data(), cmd.resource_data.size(), 0);
                }
            }
            break;
        default:
            break;
        }
    }

    void RenderSystem::applySceneCommand(RenderScene& scene, SceneCommand& cmd) {
        switch (cmd.type) {
        case SceneCommandType::AddPrimitive:
            scene.addPrimitive(std::move(cmd.primitive));
            break;
        case SceneCommandType::RemovePrimitive:
            scene.removePrimitive(cmd.id);
            break;
        case SceneCommandType::UpdatePrimitiveTransform:
            scene.updatePrimitiveTransform(cmd.id, cmd.transform);
            break;
        case SceneCommandType::AddLight:
            scene.addLightSceneInfo(std::move(cmd.light));
            break;
        case SceneCommandType::RemoveLight:
            scene.removeLightSceneInfo(cmd.id);
            break;
        case SceneCommandType::UpdateLightTransform:
            scene.updateLightSceneInfoTransform(cmd.id, cmd.transform);
            break;
        case SceneCommandType::AddSprite:
            scene.addSprite(std::move(cmd.sprite));
            break;
        case SceneCommandType::RemoveSprite:
            scene.removeSprite(cmd.id);
            break;
        case SceneCommandType::UpdateSpriteTransform:
            scene.updateSpriteTransform(cmd.id, cmd.transform);
            break;
        case SceneCommandType::SubmitUIBatch:
            scene.submitUIInstances(std::move(cmd.ui_scene_infos));
            break;
        default:
            break;
        }
    }

} // namespace dodoe
