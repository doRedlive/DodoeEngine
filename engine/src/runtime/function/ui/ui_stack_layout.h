// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"
#include "ui_types.h"

namespace dodoe {

    class UIStackLayout : public UIElement {
    private:
        LayoutDirection m_direction{LayoutDirection::Vertical};
        Float m_spacing{0};
        Alignment m_child_alignment{Alignment::Start};

    public:
        void setDirection(LayoutDirection dir) { m_direction = dir; invalidateLayout(); }
        void setSpacing(Float spacing) { m_spacing = spacing; invalidateLayout(); }
        void setChildAlignment(Alignment align) { m_child_alignment = align; invalidateLayout(); }

    protected:
        void onLayout() override;
    };

} // namespace dodoe
