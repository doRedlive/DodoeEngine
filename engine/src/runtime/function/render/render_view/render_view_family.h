// do@Redlive

#pragma once

#include "dopch.h"

#include "render_view.h"

namespace dodoe {

    class RenderScene;

    class RenderViewFamily {
        DynamicArray<RenderView> m_views{};
        Float m_time_seconds{0.0f};
        Float m_delta_seconds{0.0f};

    public:
        RenderViewFamily() = default;

        RenderView& createView(Identifier id);

        void setFrameTime(const Float time_seconds, const Float delta_seconds) {
            m_time_seconds = time_seconds;
            m_delta_seconds = delta_seconds;
        }

        void buildVisiblePrimitives(const RenderScene& scene);

        [[nodiscard]] Bool isEmpty() const { return m_views.empty(); }
        [[nodiscard]] Size_t getSize() const { return m_views.size(); }
        [[nodiscard]] DynamicArray<RenderView>& getViews() { return m_views; }
        [[nodiscard]] const DynamicArray<RenderView>& getViews() const { return m_views; }
        [[nodiscard]] RenderView& getView(const Size_t index) { return m_views[index]; }
        [[nodiscard]] const RenderView& getView(const Size_t index) const { return m_views[index]; }
        [[nodiscard]] Float getTimeSeconds() const { return m_time_seconds; }
        [[nodiscard]] Float getDeltaSeconds() const { return m_delta_seconds; }
    };

} // dodoe
