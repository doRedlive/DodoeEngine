//
// Created by GreenMuffin on 2025/10/18.
//

#ifndef DODOE_APPLICATION_H
#define DODOE_APPLICATION_H

#include "dopch.h"
#include "runtime/core/layer/layer_stack.h"

#include "function/render/render_api.h"

namespace dodoe {

    struct ApplicationCommandLineArgs {
        int argc{ 0 };
        char** args{ nullptr };
    };

    struct ApplicationSpecification {
        std::string name{ "Dodoe Engine" };

        ui32 width{ 1920 };
        ui32 height{ 1080 };

        bool window_resizeable{ true };
        bool custom_titlebar{ false };

        RenderApiType render_api_type{ RenderApiType::OpenGL };

        ApplicationCommandLineArgs cli_args{};
    };

    class Application {
    public:
        explicit Application(const ApplicationSpecification& spec);
        virtual ~Application();

        static Application& self() { return *instance_; }
        const ApplicationSpecification& specification() { return app_spec_; }

        void run();
        virtual void tick_one_frame();
        virtual void update(float delta_time);
        virtual void render();

        virtual void push_layer(Layer* layer);
        virtual void pop_layer(Layer* layer);

    protected:
        void push_runtime_layer();
        void pop_runtime_layer();

    private:
        bool running {false};
        LayerStack layer_stack_ {};
        Layer* runtime_layer_{ nullptr };
        ApplicationSpecification app_spec_{};
        static Application* instance_;

        [[nodiscard]]
        bool initialize_();
        [[nodiscard]]
        bool shutdown_();

        void quit_();
    };

    Application* create_application(ApplicationCommandLineArgs cli_args);
} // dodoe


#endif //DODOE_APPLICATION_H
