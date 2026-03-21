//
// Created by GreenMuffin on 2025/11/08.
//

#ifndef DODOE_UI_SYSTEM_H
#define DODOE_UI_SYSTEM_H

namespace dodoe {
    class WindowManager;

    class UiSystem {
    public:
        UiSystem() = default;
        ~UiSystem() = default;

        void initialize(WindowManager* window_manager);
        void shutdown();

        void begin_render();
        void end_render();
        
    private:
    };
} // dodoe

#endif // DODOE_UI_SYSTEM_H
