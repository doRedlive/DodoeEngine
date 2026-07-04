// do@Redlive

#include "application.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/thread/task_scheduler.h"

namespace dodoe {

    Application* Application::m_instance = nullptr;

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