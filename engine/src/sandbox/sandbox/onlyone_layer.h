//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_ONLYONE_LAYER_H
#define DODOE_ONLYONE_LAYER_H

#include "runtime/core/layer/layer.h"

namespace sandbox {

    class OnlyoneLayer : public dodoe::Layer {
    public:
        explicit OnlyoneLayer(const std::string& name) : dodoe::Layer(name) { }
        ~OnlyoneLayer() override = default;

        void on_attach() override;
        void on_detach() override;
        void on_update(float dt) override;
        void on_render() override;

    private:
    };

} // sandbox


#endif//DODOE_ONLYONE_LAYER_H