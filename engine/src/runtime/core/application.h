// do@Redlive

#pragma once

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

        RenderApiType render_api_type{ RenderApiType::Vulkan };
        RenderGraphMode render_graph_mode{ RenderGraphMode::TwoD };

        ApplicationCommandLineArgs cli_args{};
    };

    class Application {
        static Application* m_instance;
        Bool m_running {false};
        ApplicationSpecification m_app_spec{};
    protected:
        Scope<SystemContext> m_context{nullptr};
    public:
        explicit Application(const ApplicationSpecification& spec);
        virtual ~Application();

        Application(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(const Application&) = delete;
        Application& operator=(Application&&) = delete;

        [[nodiscard]] static Application& Self() { return *m_instance; }
        [[nodiscard]] const ApplicationSpecification& specification() { return m_app_spec; }
        [[nodiscard]] SystemContext& context();
        [[nodiscard]] const SystemContext& context() const;

        void run();

    private:
        void quit();
    };

    Application* CreateApplication(ApplicationCommandLineArgs cli_args);
} // dodoe
