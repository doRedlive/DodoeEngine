// do@Redlive

#include "application.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
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

#ifndef DODOE_EDITOR
        m_context->startRuntime();
#endif//DODOE_EDITOR

        m_context->layer_stack.attach();

        while (m_running) {
            EventSystem::Poll();
            EventSystem::Publish<BeforeOneTickEvent>();
            m_context->tickOneFrame();
            EventSystem::Publish<AfterOneTickEvent>();
            EventSystem::Handle();
        }

        m_context->layer_stack.detach();

#ifndef DODOE_EDITOR
        m_context->finalizeRuntime();
#endif//DODOE_EDITOR

        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(this);
    }

    void Application::quit() {
        m_running = false;
    }

} // dodoe
