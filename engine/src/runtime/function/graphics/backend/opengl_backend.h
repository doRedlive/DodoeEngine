// do@Redlive
#pragma once

#include "dopch.h"

#include "gfx_backend.h"

#include "GLFW/glfw3.h"

#include <mutex>
#include <thread>

namespace dodoe {

    class OpenGLBackend : public GfxBackend, public Managed<OpenGLBackend, GfxBackendCreateInfo> {
        friend class Managed<OpenGLBackend, GfxBackendCreateInfo>;

        Int32 m_fb_width{0};
        Int32 m_fb_height{0};
        std::thread::id m_context_owner{};
        std::mutex m_context_mutex{};

    public:
        [[nodiscard]] Vector2i getSwapchainExtent2d() const override { return Vector2i(m_fb_width, m_fb_height); }
        [[nodiscard]] Bool isValidationEnabled() const override { return false; }
        Bool acquireContext();
        void releaseContext();
        void updateFramebufferSize();

    private:
        Bool initialize(const GfxBackendCreateInfo& info);
        void shutdown() override;
    };

} // dodoe
