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
        DO_INFO("DrawThread Start...");
    }

    void DrawThread::stop() {
        {
            if (!m_running) return;
            m_running = false;
        }
        m_frame_queue.close();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_gfx = nullptr;
        m_device = nullptr;
    }

    void DrawThread::submit(FrameContext frame_ctx) {
        m_frame_queue.push(std::move(frame_ctx));
    }

    void DrawThread::loop() {
        while (true) {
            FrameContext frame_ctx;
            if (!m_frame_queue.pop(frame_ctx, [this] { return !m_running; })) {
                break;
            }
            m_executor.execute(m_device, m_gfx, frame_ctx);
        }
    }

} // dodoe
