// do@Redlive

#include "render_thread.h"
#include "draw_thread.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_pipeline/render_pipeline.h"

namespace dodoe {

    RenderThread::~RenderThread() {
        stop();
    }

    void RenderThread::start(RenderSystem* render_system, DrawThread* draw_thread) {
        if (m_running) return;
        m_render_system = render_system;
        m_draw_thread = draw_thread;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
        DO_DEBUG("RenderThread started");
    }

    void RenderThread::stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running) return;
            m_running = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_render_system = nullptr;
        m_draw_thread = nullptr;
    }

    void RenderThread::submitAndWait() {
        DO_DEBUG("RenderThread::submitAndWait called");
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_has_pending_frame = true;
            m_frame_completed = false;
        }
        m_cv.notify_all();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_frame_completed; });
        DO_DEBUG("RenderThread::submitAndWait completed");
    }

    void RenderThread::loop() {
        DO_DEBUG("RenderThread::loop started");
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_has_pending_frame || !m_running; });
                if (!m_running && !m_has_pending_frame) {
                    DO_DEBUG("RenderThread::loop exiting");
                    break;
                }
                m_has_pending_frame = false;
            }

            DO_DEBUG("RenderThread::loop calling renderFrame()");
            renderFrame();
            DO_DEBUG("RenderThread::loop renderFrame() completed");

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frame_completed = true;
            }
            m_cv.notify_all();
        }
    }

    void RenderThread::renderFrame() {
        auto* vp = m_render_system->getViewportManager();
        auto* gfx = m_render_system->getGfx();
        auto* pipeline = m_render_system->getRenderingPipeline();

        vp->update();
        if (vp->isWindowDirty()) {
            gfx->recreateSwapchain();
            gfx->clearGarbage();
        }
        vp->clearDirtyFlags();

        auto* scene = m_render_system->getRenderScene();
        scene->flushUpdates();

        DrawCommandList frame_commands{};

        auto* texture_manager = m_render_system->getTextureManager();
        frame_commands.append(texture_manager->flushPendingCommands());

        for (const auto& info : scene->getPrimitiveSceneInfos()) {
            const auto* obj = info.getRenderObject();
            if (obj && (obj->getRenderObjectType() == RenderObjectType::StaticMesh ||
                        obj->getRenderObjectType() == RenderObjectType::Foliage)) {
                const_cast<PrimitiveRenderObject*>(static_cast<const PrimitiveRenderObject*>(obj))
                    ->createResources(gfx->getDevice(), frame_commands);
            }
        }

        UInt32 image_index = 0;
        DO_DEBUG("RenderThread::renderFrame calling acquireNextSwapchainImage...");
        if (!gfx->acquireNextSwapchainImage(image_index)) {
            DO_DEBUG("RenderThread::renderFrame acquireNextSwapchainImage FAILED, returning early");
            return;
        }
        DO_DEBUG("RenderThread::renderFrame acquireNextSwapchainImage succeeded, image_index={}", image_index);

        auto* viewFamily = m_render_system->getViewFamily();
        viewFamily->reset();
        auto vs = vp->getPixelSize();
        RenderView main_view(Identifier{});
        main_view.setViewportRect(Vector4i(0, 0, vs.x, vs.y));
        viewFamily->addView(main_view);

        frame_commands.append(pipeline->render(*viewFamily, *scene, image_index));

        DO_DEBUG("RenderThread::renderFrame calling DrawThread::submitAndWait...");
        m_draw_thread->submitAndWait(std::move(frame_commands), image_index);
        DO_DEBUG("RenderThread::renderFrame DrawThread::submitAndWait returned");
    }

} // dodoe
