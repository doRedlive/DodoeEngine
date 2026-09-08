// do@Redlive

#include "runtime/core/config/config_system.h"

#include "runtime/core/application.h"
#include "runtime/core/project/project.h"
#include "runtime/resource/file/file_system.h"

#include <cstdlib>

namespace dodoe {

    namespace {

        constexpr std::string_view kEnvSwitches    = "DODOE_DEBUG_SWITCHES";
        constexpr std::string_view kEnvSwitchesOff = "DODOE_DEBUG_SWITCHES_OFF";
        constexpr std::string_view kArgOnEq        = "--dswitch=";
        constexpr std::string_view kArgOn          = "--dswitch";
        constexpr std::string_view kArgOffEq       = "--dswitch-off=";
        constexpr std::string_view kArgOff         = "--dswitch-off";
        constexpr std::string_view kConfigArgEq    = "--config=";
        constexpr std::string_view kConfigArg      = "--config";

        std::string_view trim(std::string_view value) {
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                value.remove_suffix(1);
            }
            return value;
        }

        Json LoadJsonFile(const FsPath& path) {
            if (!std::filesystem::exists(path)) {
                return {};
            }
            std::ifstream fin(path);
            if (!fin.is_open()) {
                DO_ERROR("ConfigSystem: failed to open config file: {}", path.string());
                return {};
            }
            try {
                Json data;
                fin >> data;
                if (!data.is_object()) {
                    DO_ERROR("ConfigSystem: config root is not an object: {}", path.string());
                    return {};
                }
                return data;
            } catch (const Json::exception& e) {
                DO_ERROR("ConfigSystem: failed to parse config file {}: {}", path.string(), e.what());
                return {};
            }
        }

    } // namespace

    void ConfigSystem::Initialize(const ApplicationCommandLineArgs& cli_args) {
        s_switches.clear();

        if (const char* env_on = std::getenv(kEnvSwitches.data())) {
            ParseSwitchList(env_on, true);
        }
        if (const char* env_off = std::getenv(kEnvSwitchesOff.data())) {
            ParseSwitchList(env_off, false);
        }

        if (cli_args.args) {
            for (int i = 0; i < cli_args.argc; ++i) {
                const std::string_view arg =
                    cli_args.args[i] ? std::string_view(cli_args.args[i]) : std::string_view();

                if (arg.starts_with(kArgOnEq)) {
                    ParseSwitchList(arg.substr(kArgOnEq.size()), true);
                    continue;
                }
                if (arg == kArgOn && i + 1 < cli_args.argc) {
                    ParseSwitchList(std::string_view(cli_args.args[++i]), true);
                    continue;
                }
                if (arg.starts_with(kArgOffEq)) {
                    ParseSwitchList(arg.substr(kArgOffEq.size()), false);
                    continue;
                }
                if (arg == kArgOff && i + 1 < cli_args.argc) {
                    ParseSwitchList(std::string_view(cli_args.args[++i]), false);
                    continue;
                }
            }
        }

        WarnUnknownSwitches();
        DO_INFO("ConfigSystem: switches [{}]", SwitchSummary());
    }

    void ConfigSystem::Shutdown() {
        s_switches.clear();
        s_app_config = {};
        s_source = ConfigSource{};
    }

    void ConfigSystem::ParseSwitchList(std::string_view list, Bool on) {
        Size_t pos = 0;
        while (pos <= list.size()) {
            const auto comma = list.find(',', pos);
            const auto token = trim(list.substr(pos,
                comma == std::string_view::npos ? list.size() - pos : comma - pos));
            if (!token.empty()) {
                s_switches.push_back(ConfigSwitchToken{ String(token.data(), token.size()), on });
            }
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
    }

    void ConfigSystem::WarnUnknownSwitches() {
        for (const ConfigSwitchToken& token : s_switches) {
            const std::string_view key(token.key.data(), token.key.size());
            if (!ConfigSwitchRegistry::IsKnown(key)) {
                DO_WARN("ConfigSystem: unknown switch '{}'", token.key);
            }
        }
    }

    Bool ConfigSystem::IsSwitchEnabled(std::string_view key) {
        for (const ConfigSwitchToken& token : s_switches) {
            if (std::string_view(token.key.data(), token.key.size()) == key) {
                return token.on;
            }
        }

        const auto dot = key.find('.');
        if (dot != std::string_view::npos) {
            const std::string_view ns = key.substr(0, dot);
            for (const ConfigSwitchToken& token : s_switches) {
                if (!token.on) continue;
                const std::string_view token_key(token.key.data(), token.key.size());
                if (token_key.starts_with(ns) &&
                    token_key.size() > ns.size() &&
                    token_key[ns.size()] == '.') {
                    return false;
                }
            }
        }

        const ConfigSwitchInfo* info = ConfigSwitchRegistry::Find(key);
        return info ? static_cast<Bool>(info->default_on) : true;
    }

    Bool ConfigSystem::IsSwitchOn(std::string_view key) {
        for (const ConfigSwitchToken& token : s_switches) {
            if (std::string_view(token.key.data(), token.key.size()) == key) {
                return token.on;
            }
        }
        return false;
    }

    Bool ConfigSystem::IsSwitchPresent(std::string_view key) {
        for (const ConfigSwitchToken& token : s_switches) {
            if (std::string_view(token.key.data(), token.key.size()) == key) {
                return true;
            }
        }
        return false;
    }

    const DynamicArray<ConfigSwitchToken>& ConfigSystem::Switches() {
        return s_switches;
    }

    String ConfigSystem::SwitchSummary() {
        if (s_switches.empty()) return "(none)";
        String out;
        for (const ConfigSwitchToken& token : s_switches) {
            if (!out.empty()) out += ", ";
            out += (token.on ? '+' : '-');
            out += token.key;
        }
        return out;
    }

    void ConfigSystem::ForEachSwitch(const std::function<void(const ConfigSwitchToken&)>& fn) {
        if (!fn) return;
        for (const ConfigSwitchToken& token : s_switches) {
            fn(token);
        }
    }

    FsPath ConfigSystem::ResolveCliConfigPath(const ApplicationCommandLineArgs& cli_args) {
        if (!cli_args.args) return {};
        for (int i = 0; i < cli_args.argc; ++i) {
            const std::string_view arg =
                cli_args.args[i] ? std::string_view(cli_args.args[i]) : std::string_view();
            if (arg.starts_with(kConfigArgEq)) {
                return FsPath(String(arg.substr(kConfigArgEq.size())));
            }
            if (arg == kConfigArg && i + 1 < cli_args.argc) {
                return FsPath(String(cli_args.args[i + 1]));
            }
        }
        return {};
    }

    Json ConfigSystem::BuildAppConfig(const ApplicationCommandLineArgs& cli_args,
                                      ConfigSource& out_source,
                                      const FsPath& explicit_path) {
        s_app_config = Json::object();
        s_source = ConfigSource{};
        out_source = s_source;

        const FsPath cli_path = ResolveCliConfigPath(cli_args);
        if (!cli_path.empty()) {
            if (!std::filesystem::exists(cli_path)) {
                DO_ERROR("ConfigSystem: application config not found: {}", cli_path.string());
                return s_app_config;
            }
            Json data = LoadJsonFile(cli_path);
            if (data.is_object()) {
                s_app_config = std::move(data);
                s_source = ConfigSource{ ConfigLayer::Cli, cli_path };
                out_source = s_source;
            }
            return s_app_config;
        }

        const FsPath builtin_path = FileSystem::GetEngineResPath() / "configs" / "app_config.json";
        Json builtin = LoadJsonFile(builtin_path);
        if (builtin.is_object()) {
            s_app_config = std::move(builtin);
            s_source = ConfigSource{ ConfigLayer::EngineBuiltin, builtin_path };
            out_source = s_source;
        }

        const Ref<Project> active_project = Project::ActiveProject();
        if (active_project) {
            const FsPath project_path = std::filesystem::absolute(
                Project::ProjectDirectory() / "app_config.json");
            Json project = LoadJsonFile(project_path);
            if (project.is_object()) {
                s_app_config.merge_patch(project);
                s_source = ConfigSource{ ConfigLayer::Project, project_path };
                out_source = s_source;
            }
        }

        if (!explicit_path.empty()) {
            Json data = LoadJsonFile(explicit_path);
            if (data.is_object()) {
                s_app_config.merge_patch(data);
                s_source = ConfigSource{ ConfigLayer::Project, explicit_path };
                out_source = s_source;
            }
        }

        return s_app_config;
    }

    const Json& ConfigSystem::AppConfigJson() {
        return s_app_config;
    }

    const ConfigSource& ConfigSystem::AppConfigSource() {
        return s_source;
    }

} // namespace dodoe
