// do@Redlive

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "runtime/core/container/containers.h"

namespace dodoe {

    struct ConfigSwitchInfo {
        std::string_view key;
        std::string_view description;
        bool             default_on{ true };
    };

    class ConfigSwitchRegistry {
    public:
        static constexpr std::size_t Count() { return k_entries.size(); }

        static const ConfigSwitchInfo& At(std::size_t index) {
            return k_entries[index < k_entries.size() ? index : 0];
        }

        static const ConfigSwitchInfo* Find(std::string_view key) {
            for (const ConfigSwitchInfo& entry : k_entries) {
                if (entry.key == key) {
                    return &entry;
                }
            }
            return nullptr;
        }

        static bool IsKnown(std::string_view key) { return Find(key) != nullptr; }

    private:
        static inline constexpr StaticArray<ConfigSwitchInfo, 10> k_entries{{
            { "imgui",                  "ImGui debug overlay",        true },
            { "server.simulation",      "Server-side simulation",     false },
            { "render.base",            "Base render pass",           true },
            { "render.lighting",        "Lighting pass",              true },
            { "render.postprocess",     "Post process pass",          true },
            { "render.sprite",          "Sprite pass",                true },
            { "render.ui",              "UI pass",                    true },
            { "render.postprocess2d",   "2D post process pass",       true },
            { "render.gizmo",           "Gizmo pass",                 true },
            { "render.present",         "Present pass",               true },
        }};
    };

} // namespace dodoe
