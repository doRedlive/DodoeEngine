// do@Redlive

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
        m_running = true;
        m_thread = std::thread(&DrawThread::loop, this);
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

    void DrawThread::submitAndWait(DrawCommandList&& command_list, const UInt32 swapchain_image_index) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frame_commands = std::move(command_list);
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
            DrawCommandList frame_commands{};
            UInt32 swapchain_image_index = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_has_pending_frame || !m_running; });
                if (!m_running && !m_has_pending_frame) {
                    break;
                }
                frame_commands = std::move(m_frame_commands);
                swapchain_image_index = m_swapchain_image_index;
                m_has_pending_frame = false;
            }

            auto gfx_cmd = m_device->createCommandList();
            if (gfx_cmd) {
                frame_commands.execute(gfx_cmd);
                m_device->executeCommandList(gfx_cmd);

                m_gfx->presentSwapchainImage(swapchain_image_index);
                m_device->runGarbageCollection();
            }

            frame_commands.reset();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frame_completed = true;
            }
            m_cv.notify_all();
        }
    }

} // dodoe
