// do@Redlive

#include "application.h"

#include "_generated/serializer/application.serializer.gen.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/resource/file/file_system.h"

#include <filesystem>
#include <fstream>

namespace dodoe {

    namespace {

        FsPath FindConfigFilePath(const ApplicationCommandLineArgs& cli_args) {
            if (!cli_args.args) return {};
            for (int i = 0; i < cli_args.argc; ++i) {
                const StringView arg = cli_args.args[i];
                if (arg == "--config" && i + 1 < cli_args.argc) {
                    return FsPath(String(cli_args.args[i + 1]));
                }
                if (arg.size() > 9 && arg.substr(0, 9) == "--config=") {
                    return FsPath(String(arg.substr(9)));
                }
            }
            return {};
        }

        FsPath ProjectConfigPath() {
            const Ref<Project> active = Project::ActiveProject();
            if (!active) {
                return {};
            }
            return std::filesystem::absolute(Project::ProjectDirectory() / "app_config.json");
        }

    } // namespace

    Application* Application::m_instance = nullptr;

    Bool ApplicationSpecification::loadFromFile(const FsPath& file_path) {
        Json data;
        try {
            std::ifstream fin(file_path);
            if (!fin.is_open()) {
                DO_ERROR("ApplicationSpecification: failed to open config file: {}", file_path.string());
                return false;
            }
            fin >> data;
        } catch (const Json::exception& e) {
            DO_ERROR("ApplicationSpecification: failed to parse config file {}: {}", file_path.string(), e.what());
            return false;
        }
        Serializer::read(data, *this);
        return true;
    }

    Bool ApplicationSpecification::saveToFile(const FsPath& file_path) const {
        std::ofstream fout(file_path);
        if (!fout.is_open()) {
            DO_ERROR("ApplicationSpecification: failed to open config file for writing: {}", file_path.string());
            return false;
        }
        fout << Serializer::write(*this).dump(4);
        return true;
    }

    void Application::loadConfigFile() {
        const FsPath cli_path = FindConfigFilePath(m_app_spec.cli_args);
        if (!cli_path.empty()) {
            if (std::filesystem::exists(cli_path)) {
                if (m_app_spec.loadFromFile(cli_path)) {
                    DO_INFO("Loaded application config from: {}", cli_path.string());
                } else {
                    DO_ERROR("Failed to load application config from: {}", cli_path.string());
                }
            } else {
                DO_ERROR("Application config not found: {}", cli_path.string());
            }
            return;
        }

        DynamicArray<FsPath> candidates;
        const FsPath project_config = ProjectConfigPath();
        if (!project_config.empty()) {
            candidates.push_back(project_config);
        }
        if (!m_app_spec.config_file.empty()) {
            candidates.push_back(m_app_spec.config_file);
        }
        candidates.push_back(FileSystem::GetEngineResPath() / "configs" / "app_config.json");

        for (const FsPath& config_path : candidates) {
            if (!std::filesystem::exists(config_path)) {
                DO_ERROR("Application config not found: {}", config_path.string());
                continue;
            }
            if (m_app_spec.loadFromFile(config_path)) {
                DO_INFO("Loaded application config from: {}", config_path.string());
            } else {
                DO_ERROR("Failed to load application config from: {}", config_path.string());
            }
            return;
        }
    }

    Application::Application(const ApplicationSpecification& spec) {
        DO_PROFILE_SCOPE_CATEGORY("Application::Application", "startup");
        m_app_spec = spec;
        loadConfigFile();
        m_context = SystemContext::Create({m_app_spec});
        m_instance = this;
        m_running = true;
        DO_INFO("Created '{}'.", m_app_spec.name);
    }

    Application::~Application() {
        DO_PROFILE_SCOPE_CATEGORY("Application::~Application", "shutdown");
        SystemContext::Destroy(m_context);
        m_instance = nullptr;
        m_running = false;
    }

    SystemContext& Application::context() {
        return *m_context;
    }

    const SystemContext& Application::context() const {
        return *m_context;
    }

    void Application::run() {
        DO_PROFILE_SCOPE_CATEGORY("Application::run", "runtime");
        DO_PROFILE_THREAD_NAME("MainThread");
        TaskScheduler::Self();

        EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(this);

        m_context->initializeModules();

        m_context->startRuntime();

        m_context->getLayerStack().attach();

        while (m_running) {
            EventSystem::Poll();
            EventSystem::Publish<BeforeOneTickEvent>();
            m_context->tickOneFrame();
            EventSystem::Publish<AfterOneTickEvent>();
            EventSystem::Handle();
        }

        m_context->getLayerStack().detach();

        m_context->stopRuntime();

        m_context->finalizeModules();

        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(this);

        m_context->postShutdown();
    }

    void Application::quit() {
        m_running = false;
    }

} // dodoe
