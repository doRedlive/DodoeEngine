//
// Created by Redlive on 2026/3/21.
//

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "runtime/function/window/window.h"

namespace dodoe {

    struct ViewportManagerCreateInfo {
        Vector2f logical{640.0f, 360.0f};
        Vector2i window{1, 1};
        Vector2i pixel{1, 1};
        Window* window_handle{nullptr};

        ViewportManagerCreateInfo() = default;
        ViewportManagerCreateInfo(Window* in_window_handle) : window_handle(in_window_handle) { }
    };

    class ViewportManager : public Managed<ViewportManager, ViewportManagerCreateInfo> {
        friend class Managed<ViewportManager, ViewportManagerCreateInfo>;
        struct LetterboxMetrics {
            Rect viewport{};
            float scale{1.0f};
        };

        Window* m_window{nullptr};

        Rect m_viewport{};
        Vector2f m_logical_size{};
        Vector2i m_window_size{};
        Vector2i m_pixel_size{};
        Vector2f m_viewport_size{};

        bool m_window_dirty{false};
        bool m_viewport_dirty{false};

    public:

        void update();
        void clearDirtyFlags();
        [[nodiscard]] bool dirty() const { return m_window_dirty || m_viewport_dirty; }
        [[nodiscard]] bool isViewportDirty() const { return m_viewport_dirty; }
        [[nodiscard]] bool isWindowDirty() const { return m_window_dirty; }
        
        void setViewportRect(const Rect& viewport_rect);
        void setLogicalSize(const Vector2f& logical);
        void setWindowSize(const Vector2i& window);
        void setPixelSize(const Vector2i& pixel);
        
        [[nodiscard]] const Vector2f& getViewportSize() const { return m_viewport_size; }
        [[nodiscard]] const Vector2f& getLogicalSize() const { return m_logical_size; }
        [[nodiscard]] const Vector2i& getWindowSize() const { return m_window_size; }
        [[nodiscard]] const Vector2i& getPixelSize() const { return m_pixel_size; }
        [[nodiscard]] const Rect& viewport() const { return m_viewport; }
        
    private:        
        bool initialize(const ViewportManagerCreateInfo& info);
        void shutdown();
        LetterboxMetrics computeLetterboxMetrics(const Vector2i& pixel_size, const Vector2f& logical_size);
    };

} // dodoe
