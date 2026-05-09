//
// Created by GreenMuffin on 2025/11/15.
//

#ifndef DODOE_LAYER_STACK_H
#define DODOE_LAYER_STACK_H
#include "dopch.h"

namespace dodoe {
    class Layer;

    class LayerStack {
    public:
        LayerStack() = default;
        ~LayerStack();

        void attach();
        void detach();

        void clear_layers();

        void push_layer(Layer* layer);
        void push_over_layer(Layer* layer);
        void pop_layer(Layer* layer);
        void pop_over_layer(Layer* layer);

        std::vector<Layer*>::iterator begin() { return layers_.begin(); }
        std::vector<Layer*>::iterator end() { return layers_.end(); }

    private:
        std::vector<Layer*> layers_ {};
        unsigned int layer_insert_index_ {0};
    };
} // dodoe


#endif //DODOE_LAYER_STACK_H
