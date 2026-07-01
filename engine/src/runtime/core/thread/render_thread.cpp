#include "render_thread.h"
#include "draw_thread.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/render_pipeline/render_pipeline.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    RenderThread::~RenderThread() {
        stop();
    }

    void RenderThread::start(RenderSystem* render_system, DrawThread* draw_thread) {
        if (m_running) return;
        m_mode = ThreadingMode::TripleThread;
        m_render_system = render_system;
        m_draw_thread = draw_thread;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
    }

    void RenderThread::start(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx) {
        if (m_running) return;
        m_mode = ThreadingMode::DualThread;
        m_render_system = render_system;
        m_device = device;
        m_gfx = gfx;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
    }

    void RenderThread::setupForDirect(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx) {
        m_mode = ThreadingMode::SingleThread;
        m_render_system = render_system;
        m_device = device;
        m_gfx = gfx;
        m_running = false;
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
        m_device = nullptr;
        m_gfx = nullptr;
    }

    void RenderThread::submitAndWait() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_has_pending_frame = true;
            m_frame_completed = false;
        }
        m_cv.notify_all();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_frame_completed; });
    }

    void RenderThread::executeFrameOnce() {
        renderFrame();
    }

    void RenderThread::loop() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_has_pending_frame || !m_running; });
                if (!m_running && !m_has_pending_frame) {
                    break;
                }
                m_has_pending_frame = false;
            }

            renderFrame();

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

        UInt32 image_index = 0;
        if (!gfx->acquireNextSwapchainImage(image_index)) {
            return;
        }

        auto* viewFamily = m_render_system->getViewFamily();
        viewFamily->reset();
        auto vs = vp->getPixelSize();
        RenderView main_view(Identifier{});
        main_view.setViewportRect(Vector4i(0, 0, vs.x, vs.y));
        viewFamily->addView(main_view);

        auto prepare_resources = [&] {
            GDrawCommandList.beginFrame();

            auto* texture_manager = m_render_system->getTextureManager();
            texture_manager->flushPendingCommands();

            for (const auto& info : scene->getPrimitiveSceneInfos()) {
                const auto* obj = info.getRenderObject();
                if (obj && (obj->getRenderObjectType() == RenderObjectType::StaticMesh ||
                            obj->getRenderObjectType() == RenderObjectType::Foliage)) {
                    const_cast<PrimitiveRenderObject*>(static_cast<const PrimitiveRenderObject*>(obj))
                        ->createResources(GDrawCommandList);
                }
            }
        };

        switch (m_mode) {
        case ThreadingMode::TripleThread:
            prepare_resources();
            pipeline->render(*viewFamily, *scene, image_index);
            m_draw_thread->submitAndWait(image_index);
            break;
        case ThreadingMode::DualThread:
        case ThreadingMode::SingleThread: {
            ImmediateFrameScope frame(m_device, m_gfx, image_index);
            prepare_resources();
            frame.flush();
            pipeline->render(*viewFamily, *scene, image_index);
            break;
        }
        }
    }

} // dodoe
