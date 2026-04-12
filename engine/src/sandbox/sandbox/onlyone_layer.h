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

        void attach() override;
        void detach() override;
        void updateTick(float dt) override;
        void renderTick() override;

    private:
    };

} // sandbox


#endif//DODOE_ONLYONE_LAYER_H