//
// Created by GreenMuffin on 2025/11/08.
//

#ifndef DODOE_UI_SYSTEM_H
#define DODOE_UI_SYSTEM_H

namespace dodoe {
    class UiSystem {
    public:
        UiSystem() = default;
        ~UiSystem() = default;

        void initialize();
        void shutdown();

        void begin_render();
        void end_render();
        
    private:
    };
} // dodoe

#endif // DODOE_UI_SYSTEM_H
