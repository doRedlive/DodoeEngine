//
// Created by GreenMuffin on 2025/10/18.
//

#include "application.h"

#include "dopch.h"

#include "runtime/core/event/event_system.h"

namespace dodoe {

    Application* Application::instance_ = nullptr;

    Application::Application(const ApplicationSpecification& spec) : app_spec_(spec) {
        DoAssert(!instance_, "Application aleady exists!");
        instance_ = this;
        context_ = SystemContext::create();
        running = true;
    }

    Application::~Application() {
        SystemContext::destroy(context_);
        instance_ = nullptr;
        running = false;
    }

    void Application::run() {
        context_->event_system->subscribe_event<ApplicationQuitEvent, &Application::quit>(this);
        context_->layer_stack.attach();
        while (running) {
            context_->event_system->poll_events();
            context_->event_system->publish_event<BeforeOneTickEvent>();
            context_->tick_one_frame();
            context_->event_system->publish_event<AfterOneTickEvent>();
            context_->event_system->handle_events();
        }
        context_->layer_stack.detach();
        context_->event_system->unsubscribe_event<ApplicationQuitEvent, &Application::quit>(this);
    }

    void Application::quit() {
        running = false;
    }

} // dodoe
