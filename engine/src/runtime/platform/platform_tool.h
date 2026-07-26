// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class PlatformTool {
    public:
        static String OpenProjectFileDialog();
        static String OpenDirectoryDialog(const String& initial_directory = {});
        static bool BuildCSharpAssembly(const FsPath& asset_directory,
            const FsPath& output_directory,
            const String& assembly_name);
    };

} // dodoe
