// do@Redlive
// Knight!

#include "render_system.h"

#include "render_settings.h"
#include "runtime/core/memory/memory.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/core/thread/draw_thread.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/time/time_system.h"

namespace dodoe {

    Bool RenderSystem::initialize(const RenderSystemCreateInfo& info) {
        m_window_manager = info.window_manager;

        auto window = m_window_manager->getWindow();
        auto backend_api = RenderSettings::GetRenderBackendApiType();

        m_view_manager = RenderViewManager::Create({m_window_manager});

        const bool enable_validation =
#ifdef DO_DEBUG
            true;
#else
            false;
#endif
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation, RenderFeatureSettings{}, window->isHostMode() ? window->getNativeHandle() : nullptr});
        GDrawCommandList.setDevice(*m_gfx);

        m_frame_scheduler = RenderFrameScheduler::Create({m_gfx->getDevice()});

        m_shared_render_service = SharedRenderService::Create({m_gfx.get()});
        m_render_scene = RenderScene::Create({m_shared_render_service.get()});
        m_render_pipeline = RenderPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_shared_render_service.get()
        });

        return m_render_scene && m_shared_render_service && m_render_pipeline && m_frame_scheduler;
    }

    void RenderSystem::shutdown() {
        m_game_command_queue.close();
        m_gfx->waitForIdle();

        RenderPipeline::Destroy(m_render_pipeline);
        RenderFrameScheduler::Destroy(m_frame_scheduler);
        RenderScene::Destroy(m_render_scene);
        SharedRenderService::Destroy(m_shared_render_service);

        RenderViewManager::Destroy(m_view_manager);

        m_gfx->waitForIdle();
        m_gfx->clearGarbage();
        GfxContext::Destroy(m_gfx);
    }

    void RenderSystem::enqueueRenderCommand(RenderCommand&& cmd) {
        m_game_command_queue.push(std::move(cmd));
    }

    void RenderSystem::renderFrame(const ThreadingMode mode, DrawThread* draw_thread) {
        Memory::AdvanceFrameEpoch();

        auto* gfx = m_gfx.get();
        auto* pipeline = m_render_pipeline.get();
        auto* view_mgr = m_view_manager.get();

        auto window = m_window_manager->getWindow();
        Vector2i cur_window(window->getWidth(), window->getHeight());
        Vector2i cur_pixel  = window->getPixelSize();

        Bool any_window_dirty = false;
        for (auto& target : view_mgr->getTargets()) {
            const auto& vp = target->getViewport();
            if (vp.getWindowSize().x != cur_window.x || vp.getWindowSize().y != cur_window.y ||
                vp.getPixelSize().x != cur_pixel.x || vp.getPixelSize().y != cur_pixel.y) {
                target->resize(cur_window, cur_pixel);
                any_window_dirty = true;
            }
        }

        if (any_window_dirty) {
            m_gfx->waitForIdle();
            m_gfx->recreateSwapchain();
            pipeline->onResize(static_cast<UInt32>(cur_pixel.x), static_cast<UInt32>(cur_pixel.y));
            m_gfx->clearGarbage();
        }

        for (auto& target : view_mgr->getTargets()) {
            target->clearGeometryDirty();
        }

        auto* scene = m_render_scene.get();

        RenderCommand cmd;
        while (m_game_command_queue.tryPop(cmd)) {
            applyRenderCommand(*scene, cmd);
        }

        UInt32 image_index = 0;
        if (!m_gfx->acquireNextSwapchainImage(image_index)) {
            return;
        }

        auto frame_ctx = m_frame_scheduler->beginFrame(image_index);

        auto* time_sys = GetTimeSystem();
        const Float frame_time = time_sys->current_time();
        const Float frame_delta = time_sys->getDeltaTime();

        frame_ctx.command_list.setRenderMode(m_gfx->getDevice());
        scene->flushUpdates(frame_ctx.command_list);

        switch (mode) {
        case ThreadingMode::TripleThread: {
            frame_ctx.command_list.beginFrame();

            for (auto& target : view_mgr->getTargets()) {
                auto* cam = target->getCamera();
                Matrix4f view = cam ? cam->getView() : Matrix4f(1.0f);
                Matrix4f proj = cam ? cam->getProj() : Matrix4f(1.0f);
                Bool show_editor = false;
#ifdef DODOE_EDITOR_ENABLED
                show_editor = cam && cam->isEditorCamera();
#endif
                auto family = target->getViewport().buildViewFamily(*scene, frame_time, frame_delta, view, proj, show_editor);
                pipeline->render(
                    family, *scene, frame_ctx.swapchain_image_index, frame_ctx.command_list,
                    frame_ctx.staging, frame_ctx.transient_resource_pool);
            }
            m_frame_scheduler->endFrame(frame_ctx);
            draw_thread->submit(std::move(frame_ctx));
            break;
        }
        case ThreadingMode::DualThread:
        case ThreadingMode::SingleThread: {
            frame_ctx.command_list.beginFrame();

            for (auto& target : view_mgr->getTargets()) {
                auto* cam = target->getCamera();
                Matrix4f view = cam ? cam->getView() : Matrix4f(1.0f);
                Matrix4f proj = cam ? cam->getProj() : Matrix4f(1.0f);
                Bool show_editor = false;
#ifdef DODOE_EDITOR_ENABLED
                show_editor = cam && cam->isEditorCamera();
#endif
                auto family = target->getViewport().buildViewFamily(*scene, frame_time, frame_delta, view, proj, show_editor);
                pipeline->render(
                    family, *scene, frame_ctx.swapchain_image_index, frame_ctx.command_list,
                    frame_ctx.staging, frame_ctx.transient_resource_pool);
            }

            {
                auto gfx_cmd = m_gfx->getDevice()->createCommandList();
                gfx_cmd->open();
                frame_ctx.command_list.execute(gfx_cmd);
                gfx_cmd->close();
                m_gfx->getDevice()->executeCommandList(gfx_cmd);
            }
            gfx->getDevice()->setEventQuery(frame_ctx.completion_query, GfxCommandQueue::Graphics);
            m_gfx->presentSwapchainImage(frame_ctx.swapchain_image_index);
            m_gfx->clearGarbage();
            frame_ctx.command_list.reset();
            break;
        }
        }
    }

    void RenderSystem::applyRenderCommand(RenderScene& scene, RenderCommand& cmd) {
        switch (cmd.type) {
        case RenderCommandType::AddPrimitive:
            scene.addPrimitive(std::move(cmd.primitive));
            break;
        case RenderCommandType::RemovePrimitive:
            scene.removePrimitive(cmd.id);
            break;
        case RenderCommandType::UpdatePrimitiveTransform:
            scene.updatePrimitiveTransform(cmd.id, cmd.transform);
            break;
        case RenderCommandType::AddLight:
            scene.addLightSceneInfo(std::move(cmd.light));
            break;
        case RenderCommandType::RemoveLight:
            scene.removeLightSceneInfo(cmd.id);
            break;
        case RenderCommandType::UpdateLightTransform:
            scene.updateLightSceneInfoTransform(cmd.id, cmd.transform);
            break;
        case RenderCommandType::AddSprite:
            scene.addSprite(std::move(cmd.sprite));
            break;
        case RenderCommandType::RemoveSprite:
            scene.removeSprite(cmd.id);
            break;
        case RenderCommandType::UpdateSpriteTransform:
            scene.updateSpriteTransform(cmd.id, cmd.transform);
            break;
        case RenderCommandType::SubmitUIBatch:
            scene.submitUIInstances(std::move(cmd.ui_scene_infos));
            break;
        default:
            break;
        }
    }

} // namespace dodoe
