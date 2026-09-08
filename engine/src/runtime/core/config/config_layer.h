// do@Redlive

#pragma once

namespace dodoe {

    enum class ConfigLayer {
        Default = 0,
        EngineBuiltin,
        Project,
        Env,
        Cli,
        Count
    };

    inline const char* ConfigLayerName(ConfigLayer layer) {
        switch (layer) {
            case ConfigLayer::Default:        return "default";
            case ConfigLayer::EngineBuiltin:  return "engine-builtin";
            case ConfigLayer::Project:        return "project";
            case ConfigLayer::Env:            return "env";
            case ConfigLayer::Cli:            return "cli";
            default:                          return "unknown";
        }
    }

} // namespace dodoe
