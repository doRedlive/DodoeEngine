#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/render_settings.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace dodoe {

    class RenderSystem;
    class DrawThread;
    class GfxContext;

    class RenderThread {
        RenderSystem* m_render_system{};
        DrawThread* m_draw_thread{};
        GfxDeviceHandle m_device{};
        GfxContext* m_gfx{};
        ThreadingMode m_mode{ThreadingMode::TripleThread};
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
        void start(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx);
        void setupForDirect(RenderSystem* render_system, GfxDeviceHandle device, GfxContext* gfx);
        void stop();
        void submitAndWait();
        void executeFrameOnce();

        [[nodiscard]] ThreadingMode getMode() const { return m_mode; }

    private:
        void loop();
        void renderFrame();
    };

} // dodoe
