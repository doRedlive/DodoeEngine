//
// Created by Redlive on 2026/3/21.
//

#include "viewport_manager.h"

#include "runtime/core/math/math.h"

namespace dodoe {
    
    Scope<ViewportManager> ViewportManager::create(ViewportManagerCreateInfo create_info) {
        auto context = create_scope<ViewportManager>();
        context->initialize(create_info);
        return context;
    }

    void ViewportManager::destroy(Scope<ViewportManager>& viewport_manager) {
        if (!viewport_manager) return;
        viewport_manager->shutdown();
        viewport_manager.reset();
    }

    void ViewportManager::initialize(ViewportManagerCreateInfo init_info) {
        dirty_ = true;
        logical_size_ = init_info.logical;
        window_size_  = init_info.window;
        pixel_size_   = init_info.pixel;
        update();
    }

    void ViewportManager::shutdown() {

    }

    void ViewportManager::update() {
        if (!dirty_) return;
        dirty_ = false;

        const auto& metrics = compute_letterbox_metrics(pixel_size_, logical_size_);
        viewport_ = metrics.viewport;
    }

    void ViewportManager::set_logical_size(const Vector2f& logical) {
        dirty_ = true;
        logical_size_ = Math::max(Vector2f(1.0f, 1.0f), logical);
    }

    void ViewportManager::set_window_size(const Vector2f& window) {
        dirty_ = true;
        window_size_ = Math::max(Vector2f(1.0f, 1.0f), window);
    }

    void ViewportManager::set_pixel_size(const Vector2f& pixel) {
        dirty_ = true;
        pixel_size_ = Math::max(Vector2f(1.0f, 1.0f), pixel);
    } 

    ViewportManager::LetterboxMetrics ViewportManager::compute_letterbox_metrics(const Vector2f& pixel_size, const Vector2f& logical_size) {
        LetterboxMetrics result{};

        if (logical_size.x <= 0.0f || logical_size.y <= 0.0f ||
            pixel_size.x <= 0.0f || pixel_size.y <= 0.0f) {
            result.viewport = Rect(Vector2f(0.0f), Math::max(Vector2f(1.0f), pixel_size));
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
        DoDebug("Scale: {}, pixel size: ({}, {})", result.scale, pixel_size.x, pixel_size.y);
        Vector2f viewport_size = logical_size * result.scale;
        viewport_size.x = std::floor(viewport_size.x);
        viewport_size.y = std::floor(viewport_size.y);
        viewport_size = Math::clamp(viewport_size, Vector2f(1.0f, 1.0f), pixel_size);
        result.viewport.size = viewport_size;
        result.viewport.pos = (pixel_size - viewport_size) * 0.5f;
        result.viewport.pos.x = std::floor(result.viewport.pos.x);
        result.viewport.pos.y = std::floor(result.viewport.pos.y);
        DoDebug("Viewport size({}, {}), pos({}, {})",
            result.viewport.size.x, result.viewport.size.y,
            result.viewport.pos.x, result.viewport.pos.y);
        return result;
    }

} // dodoe
