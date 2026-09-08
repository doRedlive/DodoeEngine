// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/config/config_layer.h"
#include "runtime/core/config/config_switch_registry.h"
#include "runtime/core/utils/json.h"

#include <functional>
#include <string_view>

namespace dodoe {

    struct ApplicationCommandLineArgs;

    struct ConfigSwitchToken {
        String key;
        Bool   on{ true };
    };

    struct ConfigSource {
        ConfigLayer layer{ ConfigLayer::Default };
        FsPath      path{};
    };

    class ConfigSystem {
    public:
        static void Initialize(const ApplicationCommandLineArgs& cli_args);
        static void Shutdown();

        [[nodiscard]] static Json BuildAppConfig(const ApplicationCommandLineArgs& cli_args,
                                                 ConfigSource& out_source,
                                                 const FsPath& explicit_path = {});
        [[nodiscard]] static FsPath ResolveCliConfigPath(const ApplicationCommandLineArgs& cli_args);

        [[nodiscard]] static const Json& AppConfigJson();
        [[nodiscard]] static const ConfigSource& AppConfigSource();

        [[nodiscard]] static Bool IsSwitchEnabled(std::string_view key);
        [[nodiscard]] static Bool IsSwitchOn(std::string_view key);
        [[nodiscard]] static Bool IsSwitchPresent(std::string_view key);
        [[nodiscard]] static const DynamicArray<ConfigSwitchToken>& Switches();
        [[nodiscard]] static String SwitchSummary();
        static void ForEachSwitch(const std::function<void(const ConfigSwitchToken&)>& fn);

    private:
        static void ParseSwitchList(std::string_view list, Bool on);
        static void WarnUnknownSwitches();

        static inline DynamicArray<ConfigSwitchToken> s_switches{};
        static inline Json         s_app_config{};
        static inline ConfigSource s_source{};
    };

} // namespace dodoe
