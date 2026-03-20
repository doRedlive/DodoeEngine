//
// Created by GreenMuffin on 2025/11/15.
//

#ifndef DODOE_LAYER_H
#define DODOE_LAYER_H
#include "dopch.h"

namespace dodoe {
    class Layer {
    public:
        explicit Layer(std::string name);

        virtual ~Layer() = default;

        virtual void on_attach() = 0;
        virtual void on_detach() = 0;
        virtual void on_update(float delta_time) = 0;
        virtual void on_ui_render() = 0;

        [[nodiscard]]
        const std::string& name() const { return name_; }

    private:
        std::string name_;
    };
} // dodoe


#endif //DODOE_LAYER_H