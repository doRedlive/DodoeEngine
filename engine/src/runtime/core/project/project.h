//
// Created by GreenMuffin on 2026/2/20.
//

#ifndef DODOE_PROJECT_H
#define DODOE_PROJECT_H

#include "dopch.h"

namespace dodoe {

	struct ProjectConfig {
		std::string name{ "Untitled" };

		std::filesystem::path start_scene_path;
		std::filesystem::path asset_directory;
		std::filesystem::path script_module_path;
	};

	class Project {
	public:
		static const std::filesystem::path& project_directory() {
			return active_project_->project_directory_;
		}

		static std::filesystem::path asset_directory() {
			return project_directory() / active_project_->config_.asset_directory;
		}

		ProjectConfig& config() { return config_; }

		static Ref<Project> active() { return active_project_; }

		static Ref<Project> create();
		static Ref<Project> load(const std::filesystem::path& path);
		static bool save_active(const std::filesystem::path& path);

	private:
		ProjectConfig config_{};
		std::filesystem::path project_directory_;

		inline static Ref<Project> active_project_;
	};
}


#endif//DODOE_PROJECT_H