// do@Redlive

#include "application.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"

namespace dodoe {

    Application* Application::m_instance = nullptr;

    Application::Application(const ApplicationSpecification& spec) : m_app_spec(spec) {
        m_context = SystemContext::create({m_app_spec});
        m_instance = this;
        m_running = true;
    }

    Application::~Application() {
        SystemContext::destroy(m_context);
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
        EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(this);
        m_context->layer_stack.attach();
        m_context->startRuntime();

        while (m_running) {
            EventSystem::Poll();
            EventSystem::Publish<BeforeOneTickEvent>();
            m_context->tickOneFrame();
            EventSystem::Publish<AfterOneTickEvent>();
            EventSystem::Handle();
        }

        m_context->finalizeRuntime();
        m_context->layer_stack.detach();
        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(this);
    }

    void Application::quit() {
        m_running = false;
    }

} // dodoe
