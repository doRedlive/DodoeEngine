// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"
#include "ui_types.h"

namespace dodoe {

    class UIGridLayout : public UIElement {
    private:
        Int m_columns{3};
        Vector2f m_spacing{0, 0};
        Vector2f m_cell_size{100, 100};

    public:
        void setColumns(Int count) { m_columns = count; invalidateLayout(); }
        void setSpacing(Vector2f spacing) { m_spacing = spacing; invalidateLayout(); }
        void setCellSize(Vector2f size) { m_cell_size = size; invalidateLayout(); }

    protected:
        void onLayout() override;
    };

} // namespace dodoe
