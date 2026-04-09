//
// Created by GreenMuffin on 2025/10/18.
//

#include "application.h"

#include "dopch.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"

namespace dodoe {

    Application* Application::instance_ = nullptr;

    Application::Application(const ApplicationSpecification& spec) : app_spec_(spec) {
        context_ = SystemContext::create({app_spec_});
        instance_ = this;
        running = true;
    }

    Application::~Application() {
        SystemContext::destroy(context_);
        instance_ = nullptr;
        running = false;
    }

    SystemContext& Application::context() {
        return *context_;
    }

    const SystemContext& Application::context() const {
        return *context_;
    }

    void Application::run() {
        EventSystem::subscribe_event<ApplicationQuitEvent, &Application::quit>(this);
        context_->layer_stack.attach();
        context_->runtime_start();

        while (running) {
            EventSystem::poll_events();
            EventSystem::publish_event<BeforeOneTickEvent>();
            context_->tick_one_frame();
            EventSystem::publish_event<AfterOneTickEvent>();
            EventSystem::handle_events();
        }

        context_->runtime_finalize();
        context_->layer_stack.detach();
        EventSystem::unsubscribe_event<ApplicationQuitEvent, &Application::quit>(this);
    }

    void Application::quit() {
        running = false;
    }

} // dodoe
