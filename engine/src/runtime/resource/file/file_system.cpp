//
// Created by GreenMuffin on 2025/11/29.
//

#include "file_system.h"

#include "runtime/core/debug/instrumentor.h"

#ifdef DO_PLATFORM_WINDOWS
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

namespace dodoe {

	const fs::path DodoeEnginePath = fs::path(DODOE_ROOT).make_preferred();
	const fs::path AssetPath = DodoeEnginePath / "engine" / "res";

	fs::path FileSystem::asset_path = AssetPath;
	std::string FileSystem::cwd_ = "";

	std::vector<char> FileSystem::readFile(const std::string& path) {
		DO_PROFILE_FUNCTION();

		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open()) {
			DoError("Open file {} failed!", path);
			return {};
		}

		const std::streamsize size = in.tellg();
		if (size <= 0) {
			return {};
		}

		std::vector<char> data(static_cast<size_t>(size));
		in.seekg(0, std::ios::beg);
		in.read(data.data(), size);
		return data;
	}

	const std::string& FileSystem::cwd() {
		if (cwd_.empty()) {
			char buffer[1024];
			if (getcwd(buffer, sizeof(buffer)) != nullptr) {
				cwd_ = std::string(buffer);
			} else {
				DoError("Get CWD fiale");
			}
		}
		return cwd_;
	}

	std::string FileSystem::str2normalize_path(const std::string& str) {
		return fs::path(str).lexically_normal().generic_string();
	}

	std::string FileSystem::path2name(const std::string& path) {
		const fs::path full_path = FileSystem::asset_path / path;
		return full_path.filename().string();
	}

	std::string FileSystem::path2name_no_ext(const std::string& path) {
		const fs::path full_path = FileSystem::asset_path / path;
		return full_path.filename().stem().string();
	}

	std::string FileSystem::relative2absolute(const std::string& path) {
		fs::path relative_path(path);
		if (!fs::is_directory(FileSystem::asset_path)) {
			DoError("Base dir must be an directory!");
		}

		fs::path absolute_path = FileSystem::asset_path / relative_path;
		return absolute_path.lexically_normal().generic_string();
	}

	std::vector<std::string> FileSystem::traverse_directory(const fs::path& target_dir, const std::vector<std::string>& extensions, bool case_sensitive) {
		std::vector<std::string> relative_path_list;
		if (!fs::exists(target_dir)) {
			DoError("The {} not exist.", target_dir.string());
			return relative_path_list;
		}
		if (!fs::is_directory(target_dir)) {
			DoError("The {} is not directory.", target_dir.string());
			return relative_path_list;
		}
		try {
			for (const fs::directory_entry& entry : fs::recursive_directory_iterator(target_dir)) {
				if (!entry.is_regular_file()) {
					continue;
				}

				std::string ext = entry.path().extension().string();
				if (!case_sensitive) {
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				}

				if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
					fs::path relative_path = fs::relative(entry.path(), FileSystem::asset_path);
					relative_path_list.emplace_back(relative_path.lexically_normal().generic_string());
				}
			}
		}
		catch (const fs::filesystem_error& err) {
			DoError("Traverse {} error occur : {}", target_dir.string(), err.what());
		}
		return relative_path_list;
	}

} // dodoe