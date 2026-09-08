// do@Redlive

#pragma once

#include "dopch.h"

#include <functional>
#include <string_view>

#include "runtime/core/config/config_system.h"

namespace dodoe {

    class DebugSwitches {
    public:
        using Token = ConfigSwitchToken;

        static Bool IsEnabled(std::string_view key) { return ConfigSystem::IsSwitchEnabled(key); }

        static Bool IsRenderFeatureEnabled(std::string_view feature_name) {
            String key{ "render." };
            key.append(feature_name);
            return IsEnabled(key);
        }

        static Bool IsImguiEnabled() { return IsEnabled("imgui"); }

        static Bool IsServerSimulationEnabled() { return IsEnabled("server.simulation"); }

        static String BuildSummary() { return ConfigSystem::SwitchSummary(); }

        static void ForEachToken(const std::function<void(const Token&)>& fn) {
            ConfigSystem::ForEachSwitch(fn);
        }
    };

} // namespace dodoe
