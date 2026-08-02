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

    namespace {

        String FindConfigFilePath(const ApplicationCommandLineArgs& cli_args) {
            if (!cli_args.args) return {};
            for (int i = 0; i < cli_args.argc; ++i) {
                const StringView arg = cli_args.args[i];
                if (arg == "--config" && i + 1 < cli_args.argc) {
                    return String(cli_args.args[i + 1]);
                }
                if (arg.size() > 9 && arg.substr(0, 9) == "--config=") {
                    return String(arg.substr(9));
                }
            }
            return {};
        }

    } // namespace

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

        const String cli_config_path = FindConfigFilePath(cli_args);
        const String config_path = cli_config_path.empty() ? String("app_config.json") : cli_config_path;

        const FsPath config_file = config_path;
        if (std::filesystem::exists(config_file)) {
            if (sandbox_spec.loadFromFile(config_file)) {
                LOG_INFO("Loaded application config from: {}", config_path);
            } else {
                LOG_ERROR("Failed to load application config from: {}", config_path);
            }
        } else if (!cli_config_path.empty()) {
            LOG_ERROR("Application config not found: {}", config_path);
        }

        return new sandbox::SandboxApp(sandbox_spec);
    }

} // dodoe