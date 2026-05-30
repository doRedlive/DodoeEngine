// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct UISystemCreateInfo { 
        WindowManager* window_manager; 
    };

    class UISystem : public Managed<UISystem, UISystemCreateInfo> {
    public:
        bool initialize(const UISystemCreateInfo& info);
        void shutdown();

        void prepare();
    };

} // dodoe
