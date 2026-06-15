// do@Redlive

#pragma once

#include "dopch.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace dodoe {

    class RenderSystem;
    class DrawThread;

    class RenderThread {
        RenderSystem* m_render_system{};
        DrawThread* m_draw_thread{};
        std::thread m_thread{};
        Bool m_has_pending_frame{false};
        Bool m_frame_completed{true};
        Bool m_running{false};
        std::mutex m_mutex{};
        std::condition_variable m_cv{};

    public:
        RenderThread() = default;
        ~RenderThread();

        RenderThread(const RenderThread&) = delete;
        RenderThread& operator=(const RenderThread&) = delete;

        void start(RenderSystem* render_system, DrawThread* draw_thread);
        void stop();
        void submitAndWait();

    private:
        void loop();
        void renderFrame();
    };

} // dodoe
