//
// Created by GreenMuffin on 2025/11/08.
//

#ifndef DODOE_UI_SYSTEM_H
#define DODOE_UI_SYSTEM_H

namespace dodoe {
    class WindowManager;

    class UiSystem {
    public:
        void initialize(WindowManager* window_manager);
        void shutdown();

        void prepare();
    };
} // dodoe

#endif // DODOE_UI_SYSTEM_H
