//
// Created by GreenMuffin on 2025/10/18.
//

#ifndef DODOE_APPLICATION_H
#define DODOE_APPLICATION_H

#include "runtime/dopch.h"

#include "runtime/function/render/render_api.h"

namespace dodoe {
    class SystemContext;

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

        [[nodiscard]] static Application& self() { return *instance_; }
        [[nodiscard]] const ApplicationSpecification& specification() { return app_spec_; }
        [[nodiscard]] SystemContext& context();
        [[nodiscard]] const SystemContext& context() const;

        void run();
    
    protected:
        Scope<SystemContext> context_{nullptr};

    private:
        bool running {false};
        ApplicationSpecification app_spec_{};
        static Application* instance_;

        void quit();
    };

    Application* create_application(ApplicationCommandLineArgs cli_args);
} // dodoe


#endif //DODOE_APPLICATION_H
