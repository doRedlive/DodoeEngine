//
// Created by GreenMuffin on 2025/11/29.
//

#pragma once

#include "dopch.h"

namespace fs = std::filesystem;

namespace dodoe {
    class FileSystem {
    public:
        inline static fs::path EngineRootPath = fs::path(DODOE_ROOT).make_preferred();
        inline static fs::path EngineResPath = EngineRootPath / "engine" / "res";;

        [[nodiscard]] static const std::string& GetCWD();
        [[nodiscard]] static const fs::path& GetDocumentsPath();
        [[nodiscard]] static const std::string GetDocumentsPathString();
        [[nodiscard]] static const fs::path& GetEngineRootPath();
        [[nodiscard]] static const std::string GetEngineRootPathString();
        [[nodiscard]] static const fs::path& GetEngineResPath();
        [[nodiscard]] static const std::string GetEngineResPathString();

        [[nodiscard]] static bool FileExists(const fs::path& path);
        [[nodisacrd]] static bool DirExists(const fs::path& path);

        [[nodiscard]] static std::string NormalizePath(const std::filesystem::path& path);

        static std::string str2normalize_path(const std::string& str);

        static std::string path2name(const std::string &path);
        static std::string path2name_no_ext(const std::string& path);
        static std::string relative2absolute(const std::string& path);
        static bool TraverseDirectory(std::vector<std::string>& out_paths, const fs::path& target_dir, const std::vector<std::string>& extensions, bool case_sensitive = false);

    private:
        static std::string s_cwd;
        static fs::path s_documents_path;
    };
} // dodoe
