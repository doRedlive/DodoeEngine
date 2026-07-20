#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_executor.h"
#include "runtime/function/render/render_frame/frame_context.h"
#include "spsc_queue.h"

#include <thread>

namespace dodoe {

    class GfxContext;

    class DrawThread {
        static constexpr Size_t kMaxFramesInFlight = 2;

        GfxDeviceHandle m_device{};
        GfxContext* m_gfx{};
        DrawExecutor m_executor{};
        SpscQueue<FrameContext, kMaxFramesInFlight> m_frame_queue;
        Bool m_running{false};
        std::thread m_thread{};

    public:
        DrawThread() = default;
        ~DrawThread();

        DrawThread(const DrawThread&) = delete;
        DrawThread& operator=(const DrawThread&) = delete;

        void start(GfxDeviceHandle device, GfxContext* gfx);
        void stop();
        void submit(FrameContext frame_ctx);

    private:
        void loop();
    };

} // dodoe
