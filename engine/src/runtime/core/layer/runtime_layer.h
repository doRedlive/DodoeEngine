//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_RUNTIME_LAYER_H
#define DODOE_RUNTIME_LAYER_H

#include "layer.h"

namespace dodoe {

    class RuntimeLayer : public Layer {
    public:
        explicit RuntimeLayer(const std::string& name) : Layer(name) { }
        ~RuntimeLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float dt) override;
        void renderTick() override;

    private:
    };

} // dodoe

#endif//DODOE_RUNTIME_LAYER_H