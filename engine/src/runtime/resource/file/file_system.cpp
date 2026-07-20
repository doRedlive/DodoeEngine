//
// Created by GreenMuffin on 2025/11/29.
//

#include "file_system.h"

#include "runtime/core/debug/instrumentor.h"

#ifdef DO_PLATFORM_WINDOWS
#include <direct.h>
#include <shlobj.h>
#include <windows.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

namespace dodoe {

    String FileSystem::s_cwd = "";
    FsPath FileSystem::s_documents_path{};
    FsPath FileSystem::s_executable_dir{};
    FsPath FileSystem::s_engine_root_path{};
    FsPath FileSystem::s_engine_res_path{};

    const String& FileSystem::GetCWD() {
        if (s_cwd.empty()) {
            char buffer[1024];
            if (getcwd(buffer, sizeof(buffer)) != nullptr) {
                s_cwd = String(buffer);
            } else {
                DO_ERROR("Get CWD fail!");
            }
        }
        return s_cwd;
    }

    const FsPath& FileSystem::GetDocumentsPath() {
        if (!s_documents_path.empty()) {
            return s_documents_path;
        }

#ifdef DO_PLATFORM_WINDOWS
        PWSTR documents_path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents_path)) && documents_path) {
            s_documents_path = FsPath(documents_path).make_preferred();
            CoTaskMemFree(documents_path);
        }
#else
        if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
            s_documents_path = (FsPath(home) / "Documents").make_preferred();
        }
#endif

        if (s_documents_path.empty()) {
#ifdef DO_PLATFORM_WINDOWS
            if (const char* user_profile = std::getenv("USERPROFILE"); user_profile && user_profile[0] != '\0') {
                s_documents_path = (FsPath(user_profile) / "Documents").make_preferred();
            }
#else
            s_documents_path = fs::current_path().make_preferred();
#endif
        }

        return s_documents_path;
    }

    String FileSystem::GetDocumentsPathString() {
        return String(GetDocumentsPath().generic_string().c_str());
    }

    const FsPath& FileSystem::GetExecutableDir() {
        if (!s_executable_dir.empty()) {
            return s_executable_dir;
        }

#ifdef DO_PLATFORM_WINDOWS
        wchar_t exe_path[MAX_PATH];
        DWORD length = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            s_executable_dir = FsPath(std::wstring(exe_path, length)).parent_path().make_preferred();
        }
#else
        s_executable_dir = fs::current_path().make_preferred();
#endif

        if (s_executable_dir.empty()) {
            s_executable_dir = fs::current_path().make_preferred();
        }

        return s_executable_dir;
    }

    String FileSystem::GetExecutableDirString() {
        return String(GetExecutableDir().generic_string().c_str());
    }

    const FsPath& FileSystem::GetEngineRootPath() {
        return GetExecutableDir();
    }

    String FileSystem::GetEngineRootPathString() {
        return String(GetEngineRootPath().generic_string().c_str());
    }

    const FsPath& FileSystem::GetEngineResPath() {
        if (!s_engine_res_path.empty()) {
            return s_engine_res_path;
        }
        s_engine_res_path = (GetExecutableDir() / "engine" / "res").make_preferred();
        return s_engine_res_path;
    }

    String FileSystem::GetEngineResPathString() {
        return String(GetEngineResPath().generic_string().c_str());
    }

    String FileSystem::NormalizePath(const FsPath& path) {
        return String(path.lexically_normal().generic_string().c_str());
    }

    Bool FileSystem::IsFileExists(const FsPath& p) {
        std::error_code ec;
        if (!fs::is_regular_file(p, ec)) {
            if (ec) {
                DO_ERROR("FileSystem::IsFileExists: {}", ec.message());
            }
            return false;
        }
        return true;
    }

    Bool FileSystem::IsDirExists(const FsPath& p) {
        std::error_code ec;
        if (!fs::is_directory(p, ec)) {
            if (ec) {
                DO_ERROR("FileSystem::IsDirExists: {}", ec.message());
            }
            return false;
        }
        return true;
    }

    String FileSystem::StrToNormalizePath(const String& str) {
        return String(FsPath(str).lexically_normal().generic_string().c_str());
    }

    String FileSystem::PathToName(const String& path) {
        return String(FsPath(path).filename().string().c_str());
    }

    String FileSystem::PathToNameNoExt(const String& path) {
        return String(FsPath(path).stem().string().c_str());
    }

    String FileSystem::RelativeToAbsolute(const String& path, const FsPath& base) {
        FsPath relative_path(path);
        if (!fs::is_directory(base)) {
            DO_ERROR("Base dir must be a directory!");
            return {};
        }

        FsPath absolute_path = base / relative_path;
        return String(absolute_path.lexically_normal().generic_string().c_str());
    }

    Bool FileSystem::TraverseDirectory(DynamicArray<String>& out_paths, const FsPath& target_dir, const DynamicArray<String>& extensions, Bool case_sensitive) {
        DynamicArray<String> relative_path_list;
        if (!fs::exists(target_dir)) {
            DO_ERROR("The {} not exist.", target_dir.string());
            out_paths = relative_path_list;
            return false;
        }
        if (!fs::is_directory(target_dir)) {
            DO_ERROR("The {} is not directory.", target_dir.string());
            out_paths = relative_path_list;
            return false;
        }
        try {
            for (const fs::directory_entry& entry : fs::recursive_directory_iterator(target_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                String ext(entry.path().extension().string().c_str());
                if (!case_sensitive) {
                    std::ranges::transform(ext, ext.begin(), ::tolower);
                }

                if (std::ranges::find(extensions, ext) != extensions.end()) {
                    FsPath relative_path = fs::relative(entry.path(), target_dir);
                    relative_path_list.emplace_back(relative_path.lexically_normal().generic_string().c_str());
                }
            }
        }
        catch (const fs::filesystem_error& err) {
            DO_ERROR("Traverse {} error occur : {}", target_dir.string(), err.what());
            out_paths = relative_path_list;
            return false;
        }
        out_paths = relative_path_list;
        return true;
    }

} // dodoe
