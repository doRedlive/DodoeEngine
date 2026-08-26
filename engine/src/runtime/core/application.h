// do@Redlive

#pragma once

#include "runtime/dopch.h"

#include "runtime/function/render/render_settings.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

REFLECTION_TYPE(ApplicationSpecification)

namespace dodoe {

    class SystemContext;

    enum class AppMode {
        Game,
        Sandbox,
        Editor,
        Server
    };

    struct DODOE_API ApplicationCommandLineArgs {
        int argc{ 0 };
        char** args{ nullptr };
    };

    STRUCT(ApplicationSpecification, WhiteListFields) {
        REFLECTION_BODY(ApplicationSpecification)

        META(Enable)
        String name{ "Dodoe Engine" };

        META(Enable)
        AppMode app_mode{ AppMode::Game };

        META(Enable)
        UInt32 width{ 1920 };
        META(Enable)
        UInt32 height{ 1080 };

        UInt32 pixel_width{ 0 };
        UInt32 pixel_height{ 0 };

        META(Enable)
        Bool window_resizeable{ true };

        void* host_handle{ nullptr };

        META(Enable)
        RenderSettingsInitInfo render_settings{};

        ApplicationCommandLineArgs cli_args{};

        FsPath config_file{};

        DODOE_API Bool loadFromFile(const FsPath& file_path);
        DODOE_API Bool saveToFile(const FsPath& file_path) const;
    };

    class DODOE_API Application {
        static Application* m_instance;
        Bool m_running {false};
        ApplicationSpecification m_app_spec{};

        void loadConfigFile();
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
        [[nodiscard]] const AppMode& getAppMode() const { return m_app_spec.app_mode; }
        [[nodiscard]] Bool isServerMode() const { return m_app_spec.app_mode == AppMode::Server; }

        void run();
        void quit();
    };

    Application* CreateApplication(ApplicationCommandLineArgs cli_args);
} // dodoe
