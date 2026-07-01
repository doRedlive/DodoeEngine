#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_executor.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace dodoe {

    class GfxContext;

    class DrawThread {
        GfxDeviceHandle m_device{};
        GfxContext* m_gfx{};
        DrawExecutor m_executor{};
        UInt32 m_swapchain_image_index{0};
        Bool m_has_pending_frame{false};
        Bool m_frame_completed{true};
        Bool m_running{false};
        std::mutex m_mutex{};
        std::condition_variable m_cv{};
        std::thread m_thread{};

    public:
        DrawThread() = default;
        ~DrawThread();

        DrawThread(const DrawThread&) = delete;
        DrawThread& operator=(const DrawThread&) = delete;

        void start(GfxDeviceHandle device, GfxContext* gfx);
        void stop();
        void submitAndWait(UInt32 swapchain_image_index);

    private:
        void loop();
    };

} // dodoe
