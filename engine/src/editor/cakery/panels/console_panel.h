// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

namespace cakery {

    class ConsolePanel : public EditorPanel {
        bool m_auto_scroll{false};
        bool m_collapse_repeats{true};
        int m_filter{0};
        std::array<char, 128> m_search_buffer{};
    public:
        explicit ConsolePanel(EditorPanelDescriptor descriptor);
        void onDraw(const EditorPanelContext& context) override;
    };

} // cakery
