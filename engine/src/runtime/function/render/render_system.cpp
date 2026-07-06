// do@Redlive
// Knight!

#include "render_system.h"

#include "render_settings.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/core/thread/draw_thread.h"
#include "runtime/core/channel/render_channel.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/function/time/time_system.h"

namespace dodoe {

    bool RenderSystem::initialize(const RenderSystemCreateInfo& info) {
        m_window_manager = info.window_manager;

        auto window = m_window_manager->getWindow();
        auto backend_api = RenderSettings::GetRenderBackendApiType();

        auto main_viewport = RenderViewport::Create(RenderViewportCreateInfo{window});
        if (main_viewport) {
            m_render_viewports.push_back(std::move(main_viewport));
        }
        const bool enable_validation =
#ifdef DO_DEBUG
            true;
#else
            false;
#endif
        m_gfx = GfxContext::Create({window->getNativeWindow(), backend_api, enable_validation, window->isHostMode() ? window->getNativeHandle() : nullptr});
        GDrawCommandList.setDevice(*m_gfx);
        m_descriptor_table = DescriptorTableManager::Create({m_gfx.get()});
        m_texture_manager = TextureManager::Create({m_gfx.get(), m_descriptor_table.get()});
        m_shared_render_service = SharedRenderService::Create({m_gfx.get(), m_descriptor_table.get(), m_texture_manager.get()});
        m_render_scene = RenderScene::Create({});
        m_render_pipeline = RenderPipeline::Create({
            std::thread::hardware_concurrency(),
            m_gfx.get(),
            m_shared_render_service.get()
        });

        return m_render_scene && m_descriptor_table && m_texture_manager && m_shared_render_service && m_render_pipeline;
    }

    void RenderSystem::shutdown() {
        m_game_command_queue.close();
        m_gfx->waitForIdle();

        RenderPipeline::Destroy(m_render_pipeline);
        RenderScene::Destroy(m_render_scene);
        SharedRenderService::Destroy(m_shared_render_service);
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        for (auto& viewport : m_render_viewports) {
            RenderViewport::Destroy(viewport);
        }
        m_render_viewports.clear();
        m_gfx->waitForIdle();
        m_gfx->clearGarbage();
        GfxContext::Destroy(m_gfx);
    }

    void RenderSystem::enqueueRenderCommand(RenderCommand&& cmd) {
        m_game_command_queue.push(std::move(cmd));
    }

    void RenderSystem::renderFrame(const ThreadingMode mode, DrawThread* draw_thread) {
        auto* gfx = m_gfx.get();
        auto* pipeline = m_render_pipeline.get();

        for (auto& viewport : m_render_viewports) {
            viewport->update();
            if (viewport->isWindowDirty()) {
                gfx->recreateSwapchain();
                gfx->clearGarbage();
            }
            viewport->clearDirtyFlags();
        }

        auto* scene = m_render_scene.get();

        RenderCommand cmd;
        Int32 cmd_count = 0;
        while (m_game_command_queue.tryPop(cmd)) {
            applyRenderCommand(*scene, cmd);
            ++cmd_count;
        }

        UInt32 image_index = 0;
        if (!gfx->acquireNextSwapchainImage(image_index)) {
            return;
        }

        auto* time_sys = GetTimeSystem();
        const Float frame_time = time_sys ? time_sys->current_time() : 0.0f;
        const Float frame_delta = time_sys ? time_sys->getDeltaTime() : 0.0f;

        auto* texture_manager = m_texture_manager.get();

        switch (mode) {
        case ThreadingMode::TripleThread: {
            FrameContext frame_ctx;
            frame_ctx.swapchain_image_index = image_index;
            frame_ctx.command_list.setDevice(GDrawCommandList.getDevice());
            frame_ctx.command_list.beginFrame();

            texture_manager->flushPendingCommands();

            for (const auto& info : scene->getPrimitiveSceneInfos()) {
                const auto* obj = scene->findPrimitive(UUID{static_cast<uint64_t>(info.getId())});
                if (obj && (obj->getRenderObjectType() == RenderObjectType::StaticMesh ||
                            obj->getRenderObjectType() == RenderObjectType::Foliage)) {
                    const_cast<PrimitiveRenderObject*>(obj)->createResources(frame_ctx.command_list);
                }
            }

            scene->flushUpdates();

            auto& cam_data = GetMainCameraChannel().get<MainCameraData>();
            for (auto& viewport : m_render_viewports) {
                auto view_family = viewport->buildViewFamily(*scene, frame_time, frame_delta,
                                                              cam_data.view, cam_data.projection);
                pipeline->render(view_family, *scene, image_index, frame_ctx.command_list);
            }
            draw_thread->submit(std::move(frame_ctx));
            break;
        }
        case ThreadingMode::DualThread:
        case ThreadingMode::SingleThread: {
            ImmediateFrameScope frame(GDrawCommandList.getDevice(), gfx, image_index);

            GDrawCommandList.beginFrame();

            texture_manager->flushPendingCommands();

            for (const auto& info : scene->getPrimitiveSceneInfos()) {
                const auto* obj = scene->findPrimitive(UUID{static_cast<uint64_t>(info.getId())});
                if (obj && (obj->getRenderObjectType() == RenderObjectType::StaticMesh ||
                            obj->getRenderObjectType() == RenderObjectType::Foliage)) {
                    const_cast<PrimitiveRenderObject*>(obj)->createResources(GDrawCommandList);
                }
            }

            scene->flushUpdates();

            auto& cam_data = GetMainCameraChannel().get<MainCameraData>();
            for (auto& viewport : m_render_viewports) {
                auto view_family = viewport->buildViewFamily(*scene, frame_time, frame_delta,
                                                              cam_data.view, cam_data.projection);
                pipeline->render(view_family, *scene, image_index, GDrawCommandList);
            }
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
        default:
            break;
        }
    }

} // dodoe
