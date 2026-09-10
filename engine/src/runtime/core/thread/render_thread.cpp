// do@Redlive

#include "render_thread.h"
#include "runtime/core/memory/memory.h"
#include "runtime/core/memory/thread_allocator.h"

namespace dodoe {

    RenderThread::RenderThread(RenderFrameTask task, RenderFrameTask shutdown_task)
        : m_frame_task(std::move(task)), m_shutdown_task(std::move(shutdown_task)) {
    }

    RenderThread::~RenderThread() {
        stop();
    }

    void RenderThread::start(const Bool spawn_thread) {
        DO_PROFILE_SCOPE_CATEGORY("RenderThread::start", "startup");
        if (m_running) return;
        if (!spawn_thread) return;
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
        DO_INFO("Started.");
    }

    void RenderThread::stop() {
        DO_PROFILE_SCOPE_CATEGORY("RenderThread::stop", "shutdown");
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running) return;
            m_running = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_frame_task = nullptr;
        DO_INFO("Stopped.");
    }

    void RenderThread::submitFrame() {
        DO_PROFILE_SCOPE_CATEGORY("RenderThread::submitFrame", "frame");
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_running || m_in_flight_frames < kMaxFramesInFlight; });
        if (!m_running) return;
        ++m_pending_frames;
        ++m_in_flight_frames;
        lock.unlock();
        m_cv.notify_all();
    }

    void RenderThread::executeFrameOnce() {
        DO_PROFILE_SCOPE_CATEGORY("RenderThread::executeFrameOnce", "frame");
        if (m_frame_task) {
            m_frame_task();
        }
    }

    void RenderThread::loop() {
        DO_PROFILE_THREAD_NAME("RenderThread");
        Memory::InitThread();
        while (true) {
            UInt64 cur = Memory::CurrentFrameEpoch();
            ThreadAllocator* ta = threadAllocatorPtr();
            if (ta && ta->last_reset_epoch.exchange(cur) != cur) {
                ta->frame.reset();
            }

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_pending_frames > 0 || !m_running; });
                if (!m_running && m_pending_frames == 0) {
                    if (m_shutdown_task) {
                        m_shutdown_task();
                    }
                    Memory::ShutdownThread();
                    break;
                }
                --m_pending_frames;
            }

            if (m_frame_task) {
                DO_PROFILE_SCOPE_CATEGORY("RenderThread::frame", "frame");
                m_frame_task();
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_in_flight_frames;
            }
            m_cv.notify_all();
        }
    }

} // namespace dodoe
