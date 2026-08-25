// do@Redlive

#pragma once

#include "dopch.h"

#include "render_viewport.h"
#include "camera_provider.h"

namespace dodoe {

    struct RenderViewTargetCreateInfo {
        Vector2f logical{640.0f, 360.0f};
        Vector2i window{1, 1};
        Vector2i pixel{1, 1};
        Rect viewport_region{};
        ICameraProvider* camera{nullptr};
    };

    class RenderViewTarget : public Managed<RenderViewTarget, RenderViewTargetCreateInfo> {
        friend class Managed<RenderViewTarget, RenderViewTargetCreateInfo>;
        RenderViewport     m_viewport;
        ICameraProvider*   m_camera{nullptr};

    public:
        void resize(Vector2i window, Vector2i pixel) { m_viewport.resize(window, pixel); }
        void setLogicalSize(Vector2f logical)        { m_viewport.setLogicalSize(logical); }
        void setCamera(ICameraProvider* camera)       { m_camera = camera; }

        [[nodiscard]] const RenderViewport& getViewport() const { return m_viewport; }
        [[nodiscard]] ICameraProvider* getCamera() const        { return m_camera; }
        [[nodiscard]] Bool isGeometryDirty() const              { return m_viewport.isGeometryDirty(); }
        void clearGeometryDirty()                               { m_viewport.clearGeometryDirty(); }

    private:
        Bool initialize(const RenderViewTargetCreateInfo& info);
        void shutdown();
    };

} // namespace dodoe
