// do@Redlive

#pragma once

#include "dopch.h"

#include <thread>
#include <semaphore>
#include <functional>

namespace dodoe {

    class RenderThread {
        std::thread m_thread{};
        std::binary_semaphore m_begin_sem{0};
        std::binary_semaphore m_end_sem{0};
        Bool m_running{false};
        std::function<void()> m_work{};

    public:
        RenderThread() = default;
        ~RenderThread();

        RenderThread(const RenderThread&) = delete;
        RenderThread& operator=(const RenderThread&) = delete;

        void start(std::function<void()> work);
        void stop();
        void submitAndWait();

    private:
        void loop();
    };

} // dodoe
