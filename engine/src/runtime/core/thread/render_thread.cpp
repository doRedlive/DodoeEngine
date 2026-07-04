// do@Redlive

#include "render_thread.h"
#include "draw_thread.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    RenderThread::~RenderThread() {
        stop();
    }

    void RenderThread::start(RenderSystem* render_system, DrawThread* draw_thread) {
        if (m_running) return;
        m_mode = RenderSettings::GetThreadingMode();
        m_render_system = render_system;
        m_draw_thread = draw_thread;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
    }

    void RenderThread::start(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx) {
        if (m_running) return;
        m_mode = ThreadingMode::DualThread;
        m_render_system = render_system;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
    }

    void RenderThread::setupForDirect(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx) {
        m_mode = ThreadingMode::SingleThread;
        m_render_system = render_system;
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
        m_render_system->renderFrame(m_mode, m_draw_thread);
    }

    void RenderThread::enqueueRenderCommand(RenderCommand&& cmd) {
        m_render_system->enqueueRenderCommand(std::move(cmd));
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

            m_render_system->renderFrame(m_mode, m_draw_thread);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frame_completed = true;
            }
            m_cv.notify_all();
        }
    }

} // dodoe
