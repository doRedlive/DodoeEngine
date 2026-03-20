//
// Created by GreenMuffin on 2025/10/18.
//

#include "application.h"

#include "dopch.h"

#include "runtime/core/layer/layer.h"
#include "runtime/core/event/event_system.h"

#include "runtime/function/context.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/ui/ui_system.h"

namespace dodoe {

    Application* Application::instance_ = nullptr;

    Application::Application(const ApplicationSpecification& spec) : app_spec_(spec) {
        DoAssert(!instance_, "Application aleady exists!");
        instance_ = this;
        running = initialize_();
        DoAssert(running, "Application initialization failed!");
    }

    Application::~Application() {
        running = shutdown_();
        DoAssert(running, "Application shutdown error!");
        running = false;
        instance_ = nullptr;
    }

    void Application::run() {

        g_context.event_system->subscribe_event<ApplicationQuitEvent, &Application::quit_>(this);
        
        while (running) {
            g_context.time_system->calculate_time();
            g_context.event_system->publish_event<BeforeOneTickEvent>();
            tick_one_frame();
            g_context.event_system->publish_event<AfterOneTickEvent>();
            g_context.event_system->handle_events();
        }
        
        g_context.event_system->unsubscribe_event<ApplicationQuitEvent, &Application::quit_>(this);
    }

    void Application::tick_one_frame() {
        update(g_context.time_system->get_delta_time());
        render();
    }

    void Application::update(const float delta_time) {
        g_context.event_system->update();

        for (const auto& layer : layer_stack_) {
            layer->on_update(delta_time);
        }

        g_context.input_manager->update();
    }

    void Application::render() {
        g_context.render_system->prepare();
        g_context.ui_system->begin_render();

        for (const auto& layer : layer_stack_) {
            layer->on_ui_render();
        }
        g_context.ui_system->end_render();
        g_context.render_system->present();
    }

    void Application::push_layer(Layer* layer) {
        layer_stack_.push_layer(layer);
    }

    void Application::pop_layer(Layer* layer) {
        layer_stack_.pop_layer(layer);
    }

    bool Application::initialize_() {
        if (!g_context.initialize_systems()) {
            return false;
        }
        running = true;
        return true;
    }

    bool Application::shutdown_() {
        layer_stack_.clear_layers();
        return g_context.shutdown_systems();
    }

    void Application::quit_() {
        running = false;
    }

} // dodoe
