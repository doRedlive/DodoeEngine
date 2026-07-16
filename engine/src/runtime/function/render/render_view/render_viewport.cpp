// do@Redlive

#include "render_viewport.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_scene/render_scene.h"

namespace dodoe {

    RenderViewport::RenderViewport(Vector2f logical, Vector2i window, Vector2i pixel) {
        m_logical_size = Math::Max(Vector2f(1.0f, 1.0f), logical);
        m_window_size  = Math::Max(Vector2i(1, 1), window);
        m_pixel_size   = Math::Max(Vector2i(1, 1), pixel);
        m_viewport_size = Vector2f(static_cast<float>(m_pixel_size.x), static_cast<float>(m_pixel_size.y));

        const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
        m_viewport = metrics.viewport;
        m_geometry_dirty = true;
    }

    void RenderViewport::resize(Vector2i window_size, Vector2i pixel_size) {
        const Vector2i clamped_window = Math::Max(Vector2i(1, 1), window_size);
        const Vector2i clamped_pixel  = Math::Max(Vector2i(1, 1), pixel_size);

        if (clamped_window.x != m_window_size.x || clamped_window.y != m_window_size.y) {
            setWindowSize(clamped_window);
        }
        if (clamped_pixel.x != m_pixel_size.x || clamped_pixel.y != m_pixel_size.y) {
            setPixelSize(clamped_pixel);
        }

        if (m_geometry_dirty) {
            const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
            m_viewport = metrics.viewport;
        }
    }

    void RenderViewport::setLogicalSize(const Vector2f& logical) {
        const Vector2f clamped = Math::Max(Vector2f(1.0f, 1.0f), logical);
        if (clamped.x == m_logical_size.x && clamped.y == m_logical_size.y) {
            return;
        }

        m_logical_size = clamped;
        m_geometry_dirty = true;

        const auto metrics = computeLetterboxMetrics(m_pixel_size, m_logical_size);
        m_viewport = metrics.viewport;
    }

    void RenderViewport::setWindowSize(const Vector2i& window) {
        const Vector2i clamped = Math::Max(Vector2i(1, 1), window);
        if (clamped.x == m_window_size.x && clamped.y == m_window_size.y) {
            return;
        }

        m_window_size = clamped;
        m_geometry_dirty = true;
    }

    void RenderViewport::setPixelSize(const Vector2i& pixel) {
        const Vector2i clamped = Math::Max(Vector2i(1, 1), pixel);
        if (clamped.x == m_pixel_size.x && clamped.y == m_pixel_size.y) {
            return;
        }

        m_pixel_size = clamped;
        m_geometry_dirty = true;
    }

    void RenderViewport::setViewportRect(const Rect& viewport_rect) {
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
        m_geometry_dirty = true;
    }

    RenderViewport::LetterboxMetrics RenderViewport::computeLetterboxMetrics(const Vector2i& pixel_size_i, const Vector2f& logical_size) {
        LetterboxMetrics result{};

        const Vector2f pixel_size(static_cast<float>(pixel_size_i.x), static_cast<float>(pixel_size_i.y));

        if (logical_size.x <= 0.0f || logical_size.y <= 0.0f ||
            pixel_size.x <= 0.0f || pixel_size.y <= 0.0f) {
            result.viewport = Rect(Vector2f(0.0f), Math::Max(Vector2f(1.0f), pixel_size));
            result.scale = 1.0f;
            return result;
        }

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

    RenderViewFamily RenderViewport::buildViewFamily(const RenderScene& scene, Float time, Float delta,
        const Matrix4f& view_mat, const Matrix4f& proj_mat, Bool show_editor_primitives) const {
        RenderViewFamily family{};
        family.setFrameTime(time, delta);
        auto& view = family.createView(Identifier("main_view"));
        view.setMatrices(view_mat, proj_mat);
        view.setViewportRect(Vector4i(
            static_cast<int>(m_viewport.pos.x),
            static_cast<int>(m_viewport.pos.y),
            static_cast<int>(m_viewport.size.x),
            static_cast<int>(m_viewport.size.y)
        ));
        if (show_editor_primitives) {
            view.enableViewFlag(RenderView::kShowEditorPrimitives);
        }
        return family;
    }

    Vector2f RenderViewport::Window2World(const Vector2f& window_pos, const Matrix4f& view_proj) const {
        if (m_window_size.x <= 0 || m_window_size.y <= 0
            || m_pixel_size.x <= 0 || m_pixel_size.y <= 0
            || m_viewport.size.x <= 0.0f || m_viewport.size.y <= 0.0f) {
            return window_pos;
        }

        const Vector2f pixel_pos{
            window_pos.x * (static_cast<float>(m_pixel_size.x) / static_cast<float>(m_window_size.x)),
            window_pos.y * (static_cast<float>(m_pixel_size.y) / static_cast<float>(m_window_size.y))
        };

        const float normalized_x = (pixel_pos.x - m_viewport.pos.x) / m_viewport.size.x;
        const float normalized_y = (pixel_pos.y - m_viewport.pos.y) / m_viewport.size.y;

        const Vector3f logical_pos{
            normalized_x * m_logical_size.x,
            (1.0f - normalized_y) * m_logical_size.y,
            0.0f
        };

        const Vector4f clip_pos{
            logical_pos.x / m_logical_size.x * 2.0f - 1.0f,
            logical_pos.y / m_logical_size.y * 2.0f - 1.0f,
            0.0f,
            1.0f
        };
        Vector4f world_pos = Math::Inverse(view_proj) * clip_pos;
        if (std::abs(world_pos.w) > 0.00001f) {
            world_pos /= world_pos.w;
        }
        return Vector2f(world_pos.x, world_pos.y);
    }

} // namespace dodoe
