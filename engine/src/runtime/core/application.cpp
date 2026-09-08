// do@Redlive

#include "application.h"

#include "_generated/serializer/application.serializer.gen.h"

#include "runtime/core/config/config_system.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/resource/file/file_system.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace dodoe {

    namespace {

        constexpr UInt32 kDefaultSmokeFrames = 60;

#ifndef DODOE_SHIPPING
        UInt32 ParseSmokeFrames(const ApplicationCommandLineArgs& cli_args) {
            if (!cli_args.args) return 0;
            for (int i = 0; i < cli_args.argc; ++i) {
                const StringView arg = cli_args.args[i];
                if (arg == "--smoke-test") {
                    return kDefaultSmokeFrames;
                }
                if (arg.size() >= 13 && arg.substr(0, 13) == "--smoke-test=") {
                    const StringView value = arg.substr(13);
                    UInt32 frames = kDefaultSmokeFrames;
                    if (!value.empty()) {
                        const char* first = value.data();
                        const char* last = value.data() + value.size();
                        const auto result = std::from_chars(first, last, frames);
                        if (result.ec != std::errc() || frames == 0) {
                            return kDefaultSmokeFrames;
                        }
                    }
                    return frames;
                }
            }
            return 0;
        }
#endif

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
        return loadFromJson(data);
    }

    Bool ApplicationSpecification::loadFromJson(const Json& data) {
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
        ConfigSource source{};
        const Json config_data = ConfigSystem::BuildAppConfig(
            m_app_spec.cli_args, source, m_app_spec.config_file);
        if (source.layer == ConfigLayer::Default) {
            DO_ERROR("Application config not found (no cli/project/engine-builtin config)");
            return;
        }
        m_app_spec.loadFromJson(config_data);
        m_app_spec.config_file = source.path;
        DO_INFO("Loaded application config [layer:{}] from: {}",
            ConfigLayerName(source.layer), source.path.string());
    }

    Application::Application(const ApplicationSpecification& spec) {
        DO_PROFILE_SCOPE_CATEGORY("Application::Application", "startup");
        ConfigSystem::Initialize(spec.cli_args);
        m_app_spec = spec;
#ifndef DODOE_SHIPPING
        m_smoke_frames = ParseSmokeFrames(spec.cli_args);
#endif
        loadConfigFile();
        if (m_app_spec.app_mode == AppMode::Server) {
            m_app_spec.render_settings.windowless = true;
        }
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
        ConfigSystem::Shutdown();
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

#ifndef DODOE_SHIPPING
        UInt32 frames_run = 0;
#endif
        while (m_running) {
            EventSystem::Publish<BeforeOneTickEvent>();
            if (auto* time_system = m_context->getTimeSystem()) {
                time_system->updateTime();
            }
            if (auto* input_manager = m_context->getInputManager()) {
                input_manager->beginFrame();
            }
            EventSystem::Poll();
            EventSystem::Handle();
            if (auto* input_manager = m_context->getInputManager()) {
                input_manager->update(m_context->getTimeSystem()->getDeltaTime());
            }
            m_context->tickOneFrame();
            DO_PROFILE_FRAME();
            EventSystem::Publish<AfterOneTickEvent>();
#ifndef DODOE_SHIPPING
            if (m_smoke_frames > 0 && ++frames_run >= m_smoke_frames) {
                DO_INFO("Smoke test mode: exiting after {} frames.", frames_run);
                quit();
            }
#endif
            if (isServerMode()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
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
