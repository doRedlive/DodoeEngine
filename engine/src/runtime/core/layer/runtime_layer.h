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

        void on_attach() override;
        void on_detach() override;
        void on_update(float dt) override;
        void on_render() override;

    private:
    };

} // dodoe

#endif//DODOE_RUNTIME_LAYER_H