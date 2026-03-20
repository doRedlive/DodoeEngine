//
// Sandbox runtime layer.
//

#include "sandbox_layer.h"

// #include "runtime/function/"

namespace sandbox {

SandboxLayer::SandboxLayer(const std::string& name)
    : dodoe::Layer(name) {
}

void SandboxLayer::on_attach() {
}

void SandboxLayer::on_detach() {
}

void SandboxLayer::on_update(const float delta_time) {
    (void)delta_time;
}

void SandboxLayer::on_ui_render() {
}

} // namespace sandbox
