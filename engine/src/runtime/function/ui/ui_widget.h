// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_interactive.h"

namespace dodoe {

    class UIWidget : public UIInteractive {
    private:
        Color m_color{1, 1, 1, 1};
        Float m_alpha_threshold{0};

    public:
        void setColor(Color color) { m_color = color; }
        [[nodiscard]] Color getColor() const { return m_color; }
        [[nodiscard]] Float getAlpha() const { return m_color.a; }
        void setAlpha(Float alpha) { m_color.a = alpha; }

        void setAlphaHitTestThreshold(Float threshold) { m_alpha_threshold = threshold; }
    };

} // namespace dodoe
