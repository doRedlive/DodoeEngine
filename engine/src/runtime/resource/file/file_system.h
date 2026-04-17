//
// Created by GreenMuffin on 2025/11/29.
//

#ifndef DODOE_FILE_SYSTEM_H
#define DODOE_FILE_SYSTEM_H

#include "dopch.h"

namespace fs = std::filesystem;

namespace dodoe {
    class FileSystem {
    public:
        static fs::path asset_path;
        static fs::path ScriptModulePath;

        static std::vector<char> readFile(const std::string& path);

        static const std::string& cwd();

        static std::string str2normalize_path(const std::string& str);

        static std::string path2name(const std::string &path);
        static std::string path2name_no_ext(const std::string& path);
        static std::string relative2absolute(const std::string& path);
        static std::vector<std::string> traverse_directory(const fs::path& target_dir, const std::vector<std::string>& extensions, bool case_sensitive = false);

    private:
        static std::string cwd_;
    };
} // dodoe


#endif //DODOE_FILE_SYSTEM_H