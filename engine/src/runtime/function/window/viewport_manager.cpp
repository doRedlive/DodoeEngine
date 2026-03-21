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
            result.viewport = Rect(Vector2f(0.0f), Math::max(glm::vec2(1.0f), pixel_size));
            result.scale = 1.0f;
            return result;
        }

        const float scale_x = pixel_size.x / logical_size.x;
        const float scale_y = pixel_size.y / logical_size.y;
        result.scale = std::min(scale_x, scale_y);

        if (!std::isfinite(result.scale) || result.scale <= 0.0f) {
            result.scale = 1.0f;
        }

        Vector2f viewport_size = logical_size * result.scale;
        viewport_size = Math::clamp(viewport_size, Vector2f(1.0f, 1.0f), pixel_size);
        result.viewport.size = viewport_size;
        result.viewport.pos = (pixel_size - viewport_size) * 0.5f;
        return result;
    }

} // dodoe