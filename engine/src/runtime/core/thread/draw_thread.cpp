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

    void DrawThread::submitAndWait(DrawCommandList&& command_list, const UInt32 swapchain_image_index) {
        fprintf(stderr, "[DThread] submitAndWait signal\n"); fflush(stderr);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frame_commands = std::move(command_list);
            m_swapchain_image_index = swapchain_image_index;
            m_has_pending_frame = true;
            m_frame_completed = false;
        }
        m_cv.notify_all();

        fprintf(stderr, "[DThread] waiting for frame complete...\n"); fflush(stderr);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_frame_completed; });
        fprintf(stderr, "[DThread] frame complete received\n"); fflush(stderr);
    }

    void DrawThread::loop() {
        while (true) {
            DrawCommandList frame_commands{};
            UInt32 swapchain_image_index = 0;
            {
                OutputDebugStringA("[DThread] loop wait...\n");
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_has_pending_frame || !m_running; });
                if (!m_running && !m_has_pending_frame) {
                    OutputDebugStringA("[DThread] loop exit\n");
                    break;
                }
                { char _b[64]; snprintf(_b, sizeof(_b), "[DThread] loop woke image=%u\n", m_swapchain_image_index); OutputDebugStringA(_b); }
                frame_commands = std::move(m_frame_commands);
                swapchain_image_index = m_swapchain_image_index;
                m_has_pending_frame = false;
            }

            fprintf(stderr, "[DThread] creating command list...\n"); fflush(stderr);
            auto gfx_cmd = m_device->createCommandList(cutie::CommandListParameters().setEnableImmediateExecution(false));
            if (gfx_cmd) {
                fprintf(stderr, "[DThread] execute commands...\n"); fflush(stderr);
                frame_commands.execute(gfx_cmd);
                fprintf(stderr, "[DThread] executeCommandList...\n"); fflush(stderr);
                m_device->executeCommandList(gfx_cmd);
                fprintf(stderr, "[DThread] executeCommandList done\n"); fflush(stderr);

                fprintf(stderr, "[DThread] presentSwapchainImage image=%u...\n", swapchain_image_index); fflush(stderr);
                m_gfx->presentSwapchainImage(swapchain_image_index);
                fprintf(stderr, "[DThread] presentSwapchainImage done\n"); fflush(stderr);
                m_gfx->clearGarbage();
            } else {
                fprintf(stderr, "[DThread] createCommandList FAILED\n"); fflush(stderr);
            }

            frame_commands.reset();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frame_completed = true;
            }
            OutputDebugStringA("[DThread] frame_completed=true, notify\n");
            m_cv.notify_all();
        }
    }

} // dodoe
