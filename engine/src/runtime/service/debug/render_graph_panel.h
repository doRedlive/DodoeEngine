// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class RenderGraphPanel {
    public:
        static void Register();
        static void Unregister();

    private:
        static void OnImGuiRender();
    };

} // dodoe
