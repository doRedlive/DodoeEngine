#pragma once

#include "dopch.h"

#include "runtime/function/render/render_settings.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace dodoe {

    class DrawThread;
    class GfxContext;

    using RenderFrameTask = std::function<void()>;

    class RenderThread {
        RenderFrameTask m_frame_task;
        RenderFrameTask m_shutdown_task;
        DrawThread* m_draw_thread{};
        ThreadingMode m_mode{ThreadingMode::TripleThread};
        std::thread m_thread{};
        Bool m_has_pending_frame{false};
        Bool m_frame_completed{true};
        Bool m_running{false};
        std::mutex m_mutex{};
        std::condition_variable m_cv{};

    public:
        explicit RenderThread(RenderFrameTask task, RenderFrameTask shutdown_task = {});
        ~RenderThread();

        RenderThread(const RenderThread&) = delete;
        RenderThread& operator=(const RenderThread&) = delete;

        void start(ThreadingMode mode);
        void stop();
        void submit();
        void submitAndWait();
        void executeFrameOnce();

        [[nodiscard]] ThreadingMode getMode() const { return m_mode; }

    private:
        void loop();
    };

} // namespace dodoe
