//
// Created by GreenMuffin on 2025/11/15.
//

#ifndef DODOE_LAYER_H
#define DODOE_LAYER_H
#include "dopch.h"

namespace dodoe {
    class Layer {
        std::string name_;
    public:
        explicit Layer(std::string name);

        virtual ~Layer() = default;

        virtual void attach() = 0;
        virtual void detach() = 0;
        virtual void updateTick(float delta_time) = 0;
        virtual void renderTick() = 0;

        [[nodiscard]]
        const std::string& name() const { return name_; }
    };
} // dodoe


#endif //DODOE_LAYER_H