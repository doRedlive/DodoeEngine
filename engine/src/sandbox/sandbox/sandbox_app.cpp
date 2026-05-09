//
// Sandbox application entry registration.
//

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "sandbox_layer.h"

#include "onlyone_layer.h"

namespace sandbox {

class SandboxApp final : public dodoe::Application {
public:
    explicit SandboxApp(const dodoe::ApplicationSpecification& spec)
        : dodoe::Application(spec) {
            // context_->layer_stack.push_layer(new OnlyoneLayer("Onlyone"));
            m_context->layer_stack.push_layer(new SandboxLayer("Sandbox"));
    }

    ~SandboxApp() override = default;
};

} // namespace sandbox

namespace dodoe {

Application* CreateApplication(ApplicationCommandLineArgs cli_args) {
    ApplicationSpecification sandbox_spec;
    sandbox_spec.name = "dodoe-sandbox";
    sandbox_spec.custom_titlebar = false;
    sandbox_spec.window_resizeable = true;
    sandbox_spec.width = 1600;
    sandbox_spec.height = 900;
    sandbox_spec.render_api_type = RenderApiType::Vulkan;
    sandbox_spec.cli_args = cli_args;

    return new sandbox::SandboxApp(sandbox_spec);
}

} // namespace dodoe
