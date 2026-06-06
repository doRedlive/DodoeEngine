// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/render/render_settings.h"

namespace dodoe {

    class SystemContext;

    struct DODOE_API ApplicationCommandLineArgs {
        int argc{ 0 };
        char** args{ nullptr };
    };

    struct DODOE_API ApplicationSpecification {
        String name{ "Dodoe Engine" };

        ui32 width{ 1920 };
        ui32 height{ 1080 };

        Bool window_resizeable{ true };
        Bool custom_titlebar{ false };

        Size_t thread_pool_size{ 0 };
        Bool enable_async_loading{ false };

        RenderSettingsInitInfo render_settings{};

        ApplicationCommandLineArgs cli_args{};
    };

    class DODOE_API Application {
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
        void quit();
    };

    Application* CreateApplication(ApplicationCommandLineArgs cli_args);
} // dodoe
