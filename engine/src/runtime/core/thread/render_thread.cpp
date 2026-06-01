// do@Redlive

#include "render_thread.h"

namespace dodoe {

    RenderThread::~RenderThread() {
        stop();
    }

    void RenderThread::start(std::function<void()> work) {
        if (m_running) {
            return;
        }
        m_work = std::move(work);
        m_running = true;
        m_thread = std::thread(&RenderThread::loop, this);
    }

    void RenderThread::stop() {
        if (!m_running) {
            return;
        }
        m_running = false;
        m_begin_sem.release();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void RenderThread::submitAndWait() {
        m_begin_sem.release();
        m_end_sem.acquire();
    }

    void RenderThread::loop() {
        while (m_running) {
            m_begin_sem.acquire();
            if (!m_running) {
                break;
            }
            m_work();
            m_end_sem.release();
        }
    }

} // dodoe
