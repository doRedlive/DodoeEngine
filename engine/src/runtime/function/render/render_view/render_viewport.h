// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "runtime/function/render/render_view/render_view_family.h"

namespace dodoe {

    class RenderViewport {
        struct LetterboxMetrics {
            Rect viewport{};
            float scale{1.0f};
        };

        Vector2f m_logical_size{640.0f, 360.0f};
        Vector2i m_window_size{1, 1};
        Vector2i m_pixel_size{1, 1};
        Rect     m_viewport{};
        Vector2f m_viewport_size{};
        Bool     m_geometry_dirty{true};

    public:
        RenderViewport() = default;
        RenderViewport(Vector2f logical, Vector2i window, Vector2i pixel);

        void resize(Vector2i window_size, Vector2i pixel_size);
        void clearGeometryDirty() { m_geometry_dirty = false; }

        void setViewportRect(const Rect& viewport_rect);
        void setLogicalSize(const Vector2f& logical);
        void setWindowSize(const Vector2i& window);
        void setPixelSize(const Vector2i& pixel);

        [[nodiscard]] const Vector2f& getViewportSize() const { return m_viewport_size; }
        [[nodiscard]] const Vector2f& getLogicalSize() const { return m_logical_size; }
        [[nodiscard]] const Vector2i& getWindowSize() const { return m_window_size; }
        [[nodiscard]] const Vector2i& getPixelSize() const { return m_pixel_size; }
        [[nodiscard]] const Rect& getViewportRect() const { return m_viewport; }
        [[nodiscard]] Bool isGeometryDirty() const { return m_geometry_dirty; }

        [[nodiscard]] RenderViewFamily buildViewFamily(const class RenderScene& scene, Float time, Float delta,
            const Matrix4f& view, const Matrix4f& proj, Bool show_editor_primitives = false) const;
        [[nodiscard]] Vector2f Window2World(const Vector2f& window_pos, const Matrix4f& view_proj) const;

    private:
        LetterboxMetrics computeLetterboxMetrics(const Vector2i& pixel_size, const Vector2f& logical_size);
    };

} // namespace dodoe
