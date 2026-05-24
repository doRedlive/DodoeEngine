//
// Created by Redlive on 2026/3/21.
//

#include "viewport_manager.h"

#include "runtime/core/math/math.h"

namespace dodoe {
    
    bool ViewportManager::initialize(const ViewportManagerCreateInfo& init_info) {
        m_window = init_info.window_handle;
        m_logical_size = Math::Max(Vector2f(1.0f, 1.0f), init_info.logical);
        m_window_size = Math::Max(Vector2i(1, 1), init_info.window);
        m_pixel_size = Math::Max(Vector2i(1, 1), init_info.pixel);
        m_viewport_size = Vector2f(static_cast<float>(m_pixel_size.x), static_cast<float>(m_pixel_size.y));

        const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
        m_viewport = metrics.viewport;
        m_window_dirty = true;
        m_viewport_dirty = true;
        return true;
    }

    void ViewportManager::shutdown() {
        m_window_dirty = false;
        m_viewport_dirty = false;
    }

    void ViewportManager::update() {
        const Vector2i new_window_size(m_window->width(), m_window->height());
        const Vector2i new_pixel_size = m_window->pixelSize();

        if (new_window_size.x != m_window_size.x || new_window_size.y != m_window_size.y) {
            setWindowSize(new_window_size);
        }
        if (new_pixel_size.x != m_pixel_size.x || new_pixel_size.y != m_pixel_size.y) {
            setPixelSize(new_pixel_size);
        }

        if (m_window_dirty) {
            const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
            m_viewport = metrics.viewport;
        }
    }

    void ViewportManager::clearDirtyFlags() {
        m_window_dirty = false;
        m_viewport_dirty = false;
    }

    void ViewportManager::setLogicalSize(const Vector2f& logical) {
        const Vector2f clamped = Math::Max(Vector2f(1.0f, 1.0f), logical);
        if (clamped.x == m_logical_size.x && clamped.y == m_logical_size.y) {
            return;
        }

        m_logical_size = clamped;
        m_viewport_dirty = true;

        const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
        m_viewport = metrics.viewport;
    }

    void ViewportManager::setWindowSize(const Vector2i& window) {
        const Vector2i clamped = Math::Max(Vector2i(1, 1), window);
        if (clamped.x == m_window_size.x && clamped.y == m_window_size.y) {
            return;
        }

        m_window_size = clamped;
        m_window_dirty = true;
    }

    void ViewportManager::setPixelSize(const Vector2i& pixel) {
        const Vector2i clamped = Math::Max(Vector2i(1, 1), pixel);
        if (clamped.x == m_pixel_size.x && clamped.y == m_pixel_size.y) {
            return;
        }

        m_pixel_size = clamped;
        m_window_dirty = true;
    } 

    void ViewportManager::setViewportRect(const Rect& viewport_rect) {
        const Vector2f clamped_size = Math::Max(Vector2f(1.0f, 1.0f), viewport_rect.size);
        const Vector2f window_size_f(
            static_cast<float>((std::max)(1, m_window_size.x)),
            static_cast<float>((std::max)(1, m_window_size.y))
        );
        const Vector2f pixel_size_f(
            static_cast<float>((std::max)(1, m_pixel_size.x)),
            static_cast<float>((std::max)(1, m_pixel_size.y))
        );
        const Vector2f pixel_scale(
            pixel_size_f.x / window_size_f.x,
            pixel_size_f.y / window_size_f.y
        );

        Rect pixel_rect{};
        pixel_rect.pos = Vector2f(
            std::floor(viewport_rect.pos.x * pixel_scale.x),
            std::floor(viewport_rect.pos.y * pixel_scale.y)
        );
        pixel_rect.size = Math::Max(
            Vector2f(1.0f, 1.0f),
            Vector2f(
                std::floor(clamped_size.x * pixel_scale.x),
                std::floor(clamped_size.y * pixel_scale.y)
            )
        );

        if (pixel_rect.pos.x == m_viewport.pos.x && pixel_rect.pos.y == m_viewport.pos.y &&
            pixel_rect.size.x == m_viewport.size.x && pixel_rect.size.y == m_viewport.size.y) {
            return;
        }

        m_viewport = pixel_rect;
        m_viewport_size = pixel_rect.size;
        m_viewport_dirty = true;
    }

    ViewportManager::LetterboxMetrics ViewportManager::computeLetterboxMetrics(const Vector2i& pixel_size_i, const Vector2f& logical_size) {
        LetterboxMetrics result{};

        const Vector2f pixel_size(static_cast<float>(pixel_size_i.x), static_cast<float>(pixel_size_i.y));

        if (logical_size.x <= 0.0f || logical_size.y <= 0.0f ||
            pixel_size.x <= 0.0f || pixel_size.y <= 0.0f) {
            result.viewport = Rect(Vector2f(0.0f), Math::Max(Vector2f(1.0f), pixel_size));
            result.scale = 1.0f;
            return result;
        }

        // 640 * 360 : 1920 * 1080
        // result.scale = 3 --> viewport.size = 1920 * 1080
        // 640 * 360 : 1280 * 7200 (scaled)
        // result.scale = 2 --> viewport.size = 1280 * 720

        const float scale_x = pixel_size.x / logical_size.x;
        const float scale_y = pixel_size.y / logical_size.y;
        const float raw_scale = std::min(scale_x, scale_y);
        result.scale = std::floor(raw_scale);

        if (!std::isfinite(result.scale) || result.scale < 1.0f) {
            result.scale = 1.0f;
        }
        Vector2f viewport_size = logical_size * result.scale;
        viewport_size.x = std::floor(viewport_size.x);
        viewport_size.y = std::floor(viewport_size.y);
        viewport_size = Math::Clamp(viewport_size, Vector2f(1.0f, 1.0f), pixel_size);
        result.viewport.size = viewport_size;
        result.viewport.pos = (pixel_size - viewport_size) * 0.5f;
        result.viewport.pos.x = std::floor(result.viewport.pos.x);
        result.viewport.pos.y = std::floor(result.viewport.pos.y);
        return result;
    }

} // dodoe
