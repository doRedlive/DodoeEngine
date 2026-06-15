// do@Redlive

#pragma once

#include "dopch.h"

#include "render_view.h"

namespace dodoe {

    class RenderViewFamily {
        DynamicArray<RenderView> m_views{};
        Float m_time_seconds{0.0f};
        Float m_delta_seconds{0.0f};

    public:
        RenderViewFamily() = default;

        void reset() {
            m_views.clear();
            m_time_seconds = 0.0f;
            m_delta_seconds = 0.0f;
        }
        void addView(const RenderView& view) {
            m_views.push_back(view);
        }
        void setFrameTime(const Float time_seconds, const Float delta_seconds) {
            m_time_seconds = time_seconds;
            m_delta_seconds = delta_seconds;
        }

        [[nodiscard]] Bool isEmpty() const { return m_views.empty(); }
        [[nodiscard]] Size_t size() const { return m_views.size(); }
        [[nodiscard]] DynamicArray<RenderView>& getViews() { return m_views; }
        [[nodiscard]] const DynamicArray<RenderView>& getViews() const { return m_views; }
        [[nodiscard]] RenderView& getView(const Size_t index) { return m_views[index]; }
        [[nodiscard]] const RenderView& getView(const Size_t index) const { return m_views[index]; }
        [[nodiscard]] Float getTimeSeconds() const { return m_time_seconds; }
        [[nodiscard]] Float getDeltaSeconds() const { return m_delta_seconds; }
    };

} // dodoe
