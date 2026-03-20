//
// Minimal runtime layer placeholder.
//

#ifndef DODOE_RUNTIME_LAYER_H
#define DODOE_RUNTIME_LAYER_H

#include "runtime/core/layer/layer.h"

namespace dodoe {

class RuntimeLayer : public Layer {
public:
    explicit RuntimeLayer(const std::string& name) : Layer(name) {}

    void on_attach() override {}
    void on_detach() override {}
    void on_update(float) override {}
    void on_ui_render() override {}
};

} // namespace dodoe

#endif // DODOE_RUNTIME_LAYER_H
