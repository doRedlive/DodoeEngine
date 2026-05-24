// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class PlatformTool {
    public:
        static std::string OpenProjectFileDialog();
        static std::string OpenDirectoryDialog(const std::string& initial_directory = {});
        static bool BuildCSharpAssembly(const std::filesystem::path& asset_directory,
            const std::filesystem::path& output_directory,
            const std::string& assembly_name);
    };

} // dodoe
