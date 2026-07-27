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
        sandbox_spec.name = "dodoe-sandbox";

        sandbox_spec.window_resizeable = true;
        sandbox_spec.width = 1600;
        sandbox_spec.height = 900;
        sandbox_spec.render_settings.api = RenderBackendApiType::OpenGL;
        sandbox_spec.render_settings.pipeline = RenderingPipelineType::Only2D;
        sandbox_spec.render_settings.threading_mode = ThreadingMode::DualThread;
        sandbox_spec.cli_args = cli_args;

        return new sandbox::SandboxApp(sandbox_spec);
    }

} // dodoe