#include "draw_thread.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    DrawThread::~DrawThread() {
        stop();
    }

    void DrawThread::start(GfxDeviceHandle device, GfxContext* gfx) {
        if (m_running) return;
        m_device = device;
        m_gfx = gfx;
        GDrawCommandList.setDevice(device);
        m_running = true;
        m_thread = std::thread(&DrawThread::loop, this);
        DO_DEBUG("DrawThread Start...");
    }

    void DrawThread::stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running) return;
            m_running = false;
            m_frame_completed = true;
            m_has_pending_frame = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_gfx = nullptr;
        m_device = nullptr;
    }

    void DrawThread::submitAndWait(const UInt32 swapchain_image_index) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_swapchain_image_index = swapchain_image_index;
            m_has_pending_frame = true;
            m_frame_completed = false;
        }
        m_cv.notify_all();

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_frame_completed; });
    }

    void DrawThread::loop() {
        while (true) {
            UInt32 swapchain_image_index = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_has_pending_frame || !m_running; });
                if (!m_running && !m_has_pending_frame) {
                    break;
                }
                swapchain_image_index = m_swapchain_image_index;
                m_has_pending_frame = false;
            }

            m_executor.execute(m_device, m_gfx, swapchain_image_index);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frame_completed = true;
            }
            m_cv.notify_all();
        }
    }

} // dodoe
