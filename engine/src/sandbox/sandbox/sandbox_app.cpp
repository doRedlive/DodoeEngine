// do@Redlive

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "sandbox_layer.h"

namespace sandbox {

    class SandboxApp final : public dodoe::Application {
    public:
        explicit SandboxApp(const dodoe::ApplicationSpecification& spec)
            : dodoe::Application(spec) {
                m_context->getLayerStack().pushLayer(new SandboxLayer("Sandbox"));
        }

        ~SandboxApp() override = default;
    };

} // sandbox

namespace dodoe {

    Application* CreateApplication(ApplicationCommandLineArgs cli_args) {
        ApplicationSpecification sandbox_spec;
        sandbox_spec.name = "DodoeSandbox";
        sandbox_spec.app_mode = dodoe::AppMode::Sandbox;
        sandbox_spec.engine_mode = dodoe::EngineMode::TwoD;
        sandbox_spec.window_resizeable = true;
        sandbox_spec.width = 1600;
        sandbox_spec.height = 900;
        sandbox_spec.render_settings.api = RenderBackendApiType::D3D12;
        sandbox_spec.render_settings.pipeline = RenderingPipelineType::Deferred;
        sandbox_spec.cli_args = cli_args;

        return new sandbox::SandboxApp(sandbox_spec);
    }

} // dodoe