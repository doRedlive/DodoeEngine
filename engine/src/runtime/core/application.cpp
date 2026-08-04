// do@Redlive

#include "application.h"

#include "_generated/serializer/application.serializer.gen.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/async/task_scheduler.h"

#include <fstream>

namespace dodoe {

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

    Application::Application(const ApplicationSpecification& spec) : m_app_spec(spec) {
        m_context = SystemContext::Create({m_app_spec});
        m_instance = this;
        m_running = true;
    }

    Application::~Application() {
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