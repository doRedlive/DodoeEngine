// do@Redlive

#pragma once

#include "dopch.h"

namespace fs = std::filesystem;

namespace dodoe {
    class FileSystem {
        static String s_cwd;
        static FsPath s_documents_path;
        static FsPath s_executable_dir;
        static FsPath s_engine_root_path;
        static FsPath s_engine_res_path;
    public:
        [[nodiscard]] static const String& GetCWD();
        [[nodiscard]] static const FsPath& GetDocumentsPath();
        [[nodiscard]] static const String GetDocumentsPathString();
        [[nodiscard]] static const FsPath& GetExecutableDir();
        [[nodiscard]] static const String GetExecutableDirString();
        [[nodiscard]] static const FsPath& GetEngineRootPath();
        [[nodiscard]] static const String GetEngineRootPathString();
        [[nodiscard]] static const FsPath& GetEngineResPath();
        [[nodiscard]] static const String GetEngineResPathString();

        [[nodiscard]] static Bool IsFileExists(const FsPath& path);
        [[nodisacrd]] static Bool IsDirExists(const FsPath& path);

        [[nodiscard]] static String NormalizePath(const FsPath& path);

        static String StrToNormalizePath(const String& str);

        static String PathToName(const String& path);
        static String PathToNameNoExt(const String& path);
        static String RelativeToAbsolute(const String& path, const FsPath& base);
        static Bool TraverseDirectory(DynamicArray<String>& out_paths, const FsPath& target_dir, const DynamicArray<String>& extensions, Bool case_sensitive = false);
    };
} // dodoe
