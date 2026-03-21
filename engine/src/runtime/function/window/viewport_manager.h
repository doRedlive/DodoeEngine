//
// Created by Redlive on 2026/3/21.
//

#ifndef DODOE_VIEWPORT_MANAGER_H
#define DODOE_VIEWPORT_MANAGER_H

#include "dopch.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    struct ViewportManagerCreateInfo {
        Vector2f logical;
        Vector2f window;
        Vector2f pixel;
    };

    class ViewportManager {
        struct LetterboxMetrics {
            Rect viewport{};
            float scale{1.0f};
        };
    public:
        static Scope<ViewportManager> create(ViewportManagerCreateInfo create_info);
        static void destroy(Scope<ViewportManager>& viewport_manager);
        void update();
        [[nodiscard]] bool dirty() const { return dirty_; }
        [[nodiscard]] const Rect& viewport() const { return viewport_; }
        
        void set_logical_size(const Vector2f& logical);
        void set_window_size(const Vector2f& window);
        void set_pixel_size(const Vector2f& pixel);
        
        [[nodiscard]] const Vector2f& get_logical_size() const { return logical_size_; }
        [[nodiscard]] const Vector2f& get_window_size() const { return window_size_; }
        [[nodiscard]] const Vector2f& get_pixel_size() const { return pixel_size_; }
        
    private:
        Rect viewport_{};
        Vector2f logical_size_{};
        Vector2f window_size_{};
        Vector2f pixel_size_{};
        
        bool dirty_{false};
        
        void initialize(ViewportManagerCreateInfo init_info);
        void shutdown();
        LetterboxMetrics compute_letterbox_metrics(const Vector2f& pixel_size, const Vector2f& logical_size);
    };

} // dodoe

#endif//DODOE_VIEWPORT_MANAGER_H