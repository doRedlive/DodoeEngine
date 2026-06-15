// do@Redlive

#include "render_thread.h"
#include "draw_thread.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"
#include "runtime/function/render/rendering_pipeline/rendering_pipeline.h"

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
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_has_pending_frame = true;
            m_frame_completed = false;
        }
        m_cv.notify_all();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_frame_completed; });
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
            gfx->getDevice()->runGarbageCollection();
        }
        vp->clearDirtyFlags();

        Renderer::FlushSceneUpdates();
        auto& scene = Renderer::GetRenderScene();

        DrawCommandList frame_commands{};

        for (const auto& info : scene.getPrimitiveSceneInfos()) {
            const auto* obj = info.getRenderObject();
            if (obj && (obj->getRenderObjectType() == RenderObjectType::StaticMesh ||
                        obj->getRenderObjectType() == RenderObjectType::Foliage)) {
                const_cast<PrimitiveRenderObject*>(static_cast<const PrimitiveRenderObject*>(obj))
                    ->createResources(gfx->getDevice(), frame_commands);
            }
        }

        UInt32 image_index = 0;
        if (!gfx->acquireNextSwapchainImage(image_index)) return;

        RenderViewFamily view_family{};
        RenderView main_view(Identifier{});
        auto vs = vp->getPixelSize();
        main_view.setViewportRect(Vector4i(0, 0, vs.x, vs.y));
        auto* camera = Renderer::GetMainCamera();
        main_view.setMatrices(camera->getViewMatrix(), camera->getProjectionMatrix());
        view_family.addView(main_view);

        frame_commands.append(pipeline->render(view_family, scene, image_index));

        m_draw_thread->submitAndWait(std::move(frame_commands), image_index);
    }

} // dodoe
