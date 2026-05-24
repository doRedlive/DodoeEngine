// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class Layer;

    class LayerStack {
    public:
        ~LayerStack();

        void attach();
        void detach();

        void clearLayers();

        void pushLayer(Layer* layer);
        void pushOverLayer(Layer* layer);
        void popLayer(Layer* layer);
        void popOverLayer(Layer* layer);

        std::vector<Layer*>::iterator begin() { return m_layers.begin(); }
        std::vector<Layer*>::iterator end() { return m_layers.end(); }

    private:
        std::vector<Layer*> m_layers{};
        uint m_layer_insert_index{0};
    };

} // dodoe
