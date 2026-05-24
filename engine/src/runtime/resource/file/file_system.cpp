//
// Created by GreenMuffin on 2025/11/29.
//

#include "file_system.h"

#include "runtime/core/debug/instrumentor.h"

#ifdef DO_PLATFORM_WINDOWS
#include <direct.h>
#include <shlobj.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

namespace dodoe {

	std::string FileSystem::s_cwd = "";
	fs::path FileSystem::s_documents_path{};

	const std::string& FileSystem::GetCWD() {
		if (s_cwd.empty()) {
			char buffer[1024];
			if (getcwd(buffer, sizeof(buffer)) != nullptr) {
				s_cwd = std::string(buffer);
			} else {
				DO_ERROR("Get CWD fail!");
			}
		}
		return s_cwd;
	}

	const fs::path& FileSystem::GetDocumentsPath() {
		if (!s_documents_path.empty()) {
			return s_documents_path;
		}

#ifdef DO_PLATFORM_WINDOWS
		PWSTR documents_path = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents_path)) && documents_path) {
			s_documents_path = fs::path(documents_path).make_preferred();
			CoTaskMemFree(documents_path);
		}
#else
		if (const char* home = std::getenv("HOME"); home && home[0] != '\0') {
			s_documents_path = (fs::path(home) / "Documents").make_preferred();
		}
#endif

		if (s_documents_path.empty()) {
#ifdef DO_PLATFORM_WINDOWS
			if (const char* user_profile = std::getenv("USERPROFILE"); user_profile && user_profile[0] != '\0') {
				s_documents_path = (fs::path(user_profile) / "Documents").make_preferred();
			}
#else
			s_documents_path = fs::current_path().make_preferred();
#endif
		}

		return s_documents_path;
	}

	const std::string FileSystem::GetDocumentsPathString() {
		return GetDocumentsPath().generic_string();
	}

	const fs::path& FileSystem::GetEngineRootPath() {
		return EngineRootPath;
	}

	const std::string FileSystem::GetEngineRootPathString() {
		return EngineRootPath.generic_string();
	}
	const fs::path& FileSystem::GetEngineResPath() {
		return EngineResPath;
	}

	const std::string FileSystem::GetEngineResPathString() {
		return EngineResPath.generic_string();
	}
    std::string FileSystem::NormalizePath(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }

	bool FileSystem::FileExists(const std::filesystem::path& p) {
		std::error_code ec;
		if (!std::filesystem::is_regular_file(p, ec)) {
			if (ec) {
			DO_ERROR("FileSystem::FileExists: {}", ec.message());
			}
			return false;
		}
		return true;
	}

	bool FileSystem::DirExists(const std::filesystem::path& p) {
		std::error_code ec;
		if (!std::filesystem::is_directory(p, ec)) {
			if (ec) {
			DO_ERROR("FileSystem::DirExists: {}", ec.message());
			}
			return false;
		}
		return true;
	}

	std::string FileSystem::str2normalize_path(const std::string& str) {
		return fs::path(str).lexically_normal().generic_string();
	}

	std::string FileSystem::path2name(const std::string& path) {
		const fs::path full_path = FileSystem::EngineResPath / path;
		return full_path.filename().string();
	}

	std::string FileSystem::path2name_no_ext(const std::string& path) {
		const fs::path full_path = FileSystem::EngineResPath / path;
		return full_path.filename().stem().string();
	}

	std::string FileSystem::relative2absolute(const std::string& path) {
		fs::path relative_path(path);
		if (!fs::is_directory(FileSystem::EngineResPath)) {
			DO_ERROR("Base dir must be an directory!");
		}

		fs::path absolute_path = FileSystem::EngineResPath / relative_path;
		return absolute_path.lexically_normal().generic_string();
	}

	bool FileSystem::TraverseDirectory(std::vector<std::string>& out_paths, const fs::path& target_dir, const std::vector<std::string>& extensions, bool case_sensitive) {
		std::vector<std::string> relative_path_list;
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

				std::string ext = entry.path().extension().string();
				if (!case_sensitive) {
					std::ranges::transform(ext, ext.begin(), ::tolower);
				}

				if (std::ranges::find(extensions, ext) != extensions.end()) {
					fs::path relative_path = fs::relative(entry.path(), FileSystem::EngineResPath);
					relative_path_list.emplace_back(relative_path.lexically_normal().generic_string());
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
