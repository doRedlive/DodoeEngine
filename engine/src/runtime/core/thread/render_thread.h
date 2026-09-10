#pragma once

#include "dopch.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace dodoe {

    class GfxContext;

    using RenderFrameTask = std::function<void()>;

    class RenderThread {
        static constexpr Size_t kMaxFramesInFlight = 2;
        RenderFrameTask m_frame_task;
        RenderFrameTask m_shutdown_task;
        std::thread m_thread{};
        Size_t m_pending_frames{0};
        Size_t m_in_flight_frames{0};
        Bool m_running{false};
        std::mutex m_mutex{};
        std::condition_variable m_cv{};

    public:
        explicit RenderThread(RenderFrameTask task, RenderFrameTask shutdown_task = {});
        ~RenderThread();

        RenderThread(const RenderThread&) = delete;
        RenderThread& operator=(const RenderThread&) = delete;

        void start(Bool spawn_thread);
        void stop();
        void submitFrame();
        void executeFrameOnce();

    private:
        void loop();
    };

} // namespace dodoe
