//
// Sandbox runtime layer.
//

#ifndef DODOE_SANDBOX_LAYER_H
#define DODOE_SANDBOX_LAYER_H

#include "dopch.h"
#include "runtime/core/layer/layer.h"

namespace sandbox {

class SandboxLayer : public dodoe::Layer {
public:
    explicit SandboxLayer(const std::string& name);
    ~SandboxLayer() override = default;

    void on_attach() override;
    void on_detach() override;
    void on_update(float delta_time) override;
    void on_ui_render() override;
};

} // namespace sandbox

#endif // DODOE_SANDBOX_LAYER_H
