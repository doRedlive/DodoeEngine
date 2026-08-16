#include "draw_thread.h"
#include "runtime/core/memory/memory.h"
#include "runtime/core/memory/thread_allocator.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    DrawThread::~DrawThread() {
        stop();
    }

    void DrawThread::start(GfxDeviceHandle device, GfxContext* gfx) {
        DO_PROFILE_SCOPE_CATEGORY("DrawThread::start", "startup");
        if (m_running) return;
        m_device = device;
        m_gfx = gfx;
        m_running = true;
        m_thread = std::thread(&DrawThread::loop, this);
        DO_INFO("Started.");
    }

    void DrawThread::stop() {
        DO_PROFILE_SCOPE_CATEGORY("DrawThread::stop", "shutdown");
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
        DO_INFO("Stopped.");
    }

    void DrawThread::submit(FrameContext frame_ctx) {
        m_frame_queue.push(std::move(frame_ctx));
    }

    void DrawThread::loop() {
        DO_PROFILE_THREAD_NAME("DrawThread");
        Memory::InitThread();
        while (true) {
            FrameContext frame_ctx;
            if (!m_frame_queue.pop(frame_ctx, [this] { return !m_running; })) {
                break;
            }
            DO_PROFILE_SCOPE_CATEGORY("DrawThread::frame", "frame");
            m_executor.execute(m_device, m_gfx, frame_ctx);
        }
        Memory::ShutdownThread();
    }

} // dodoe
