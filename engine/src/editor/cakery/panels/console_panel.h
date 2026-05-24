// do@Redlive

#pragma once

#include "dopch.h"

namespace cakery {

    class ConsolePanel {
        bool m_auto_scroll{false};
        bool m_collapse_repeats{true};
        int m_filter{0};
        std::array<char, 128> m_search_buffer{};
    public:
        void draw();
    };

} // cakery
